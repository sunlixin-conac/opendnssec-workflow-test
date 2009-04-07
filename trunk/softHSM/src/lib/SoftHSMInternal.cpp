/* $Id$ */

/*
 * Copyright (c) 2008-2009 .SE (The Internet Infrastructure Foundation).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/************************************************************
*
* This class handles the internal state.
* Mainly session and object handling.
*
************************************************************/

#include "SoftHSMInternal.h"
#include "log.h"
#include "userhandling.h"

// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Includes for the crypto library
#include <botan/pipe.h>
#include <botan/filters.h>
#include <botan/hex.h>
#include <botan/sha2_32.h>
using namespace Botan;

SoftHSMInternal::SoftHSMInternal(bool threading, CK_CREATEMUTEX cMutex,
  CK_DESTROYMUTEX dMutex, CK_LOCKMUTEX lMutex, CK_UNLOCKMUTEX uMutex) {

  openSessions = 0;

  for(int i = 0; i < MAX_SESSION_COUNT; i++) {
    sessions[i] = NULL_PTR;
  }

  createMutexFunc = cMutex;
  destroyMutexFunc = dMutex;
  lockMutexFunc = lMutex;
  unlockMutexFunc = uMutex;
  usesThreading = threading;
  this->createMutex(&pHSMMutex);

  slots = new SoftSlot();
}

SoftHSMInternal::~SoftHSMInternal() {
  for(int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(sessions[i] != NULL_PTR) {
      // Remove the session objects created by this session
      destroySessObj(sessions[i]);
      delete sessions[i];
      sessions[i] = NULL_PTR;
    }
  }

  openSessions = 0;

  if(slots != NULL_PTR) {
    delete slots;
    slots = NULL_PTR;
  }

  this->destroyMutex(pHSMMutex);
}

int SoftHSMInternal::getSessionCount() {
  return openSessions;
}

// Creates a new session if there is enough space available.

CK_RV SoftHSMInternal::openSession(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession) {
  SoftSlot *currentSlot = slots->getSlot(slotID);

  CHECK_DEBUG_RETURN(currentSlot == NULL_PTR, "C_OpenSession", "The given slotID does not exist",
                     CKR_SLOT_ID_INVALID);
  CHECK_DEBUG_RETURN((currentSlot->slotFlags & CKF_TOKEN_PRESENT) == 0, "C_OpenSession", "The token is not present",
                     CKR_TOKEN_NOT_PRESENT);
  CHECK_DEBUG_RETURN(openSessions >= MAX_SESSION_COUNT, "C_OpenSession", "Can not open more sessions. Have reached the maximum number.",
                     CKR_SESSION_COUNT);
  CHECK_DEBUG_RETURN((flags & CKF_SERIAL_SESSION) == 0, "C_OpenSession", "Can not open a non parallel session",
                     CKR_SESSION_PARALLEL_NOT_SUPPORTED);
  CHECK_DEBUG_RETURN(!phSession, "C_OpenSession", "phSession must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);

  for(int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(sessions[i] == NULL_PTR) {
      openSessions++;
      sessions[i] = new SoftSession(flags & CKF_RW_SESSION, currentSlot);
      sessions[i]->pApplication = pApplication;
      sessions[i]->Notify = Notify;
      *phSession = (CK_SESSION_HANDLE)(i+1);

      DEBUG_MSG("C_OpenSession", "OK");
      return CKR_OK;
    }
  }

  DEBUG_MSG("C_OpenSession", "Can not open more sessions. Have reached the maximum number.");
  return CKR_SESSION_COUNT;
}

// Closes the specific session.

CK_RV SoftHSMInternal::closeSession(CK_SESSION_HANDLE hSession) {
  int sessID = hSession - 1;

  CHECK_DEBUG_RETURN(hSession > MAX_SESSION_COUNT || hSession < 1 || sessions[sessID] == NULL_PTR, "C_CloseSession", 
                     "The session does not exist", CKR_SESSION_HANDLE_INVALID);

  SoftSession *curSession = sessions[sessID];

  // Check if this is the last session on the token
  CK_BBOOL lastSessOnT = CK_TRUE;
  CK_SLOT_ID slotID = curSession->currentSlot->getSlotID();
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(sessions[i] != NULL_PTR && sessID != i) {
      if(sessions[i]->currentSlot->getSlotID() == slotID) {
        lastSessOnT = CK_FALSE;
        break;
      }
    }
  }

  // Last session for this token? Log out.
  if(lastSessOnT == CK_TRUE) {
    if(curSession->currentSlot->userPIN != NULL_PTR) {
      free(curSession->currentSlot->userPIN);
      curSession->currentSlot->userPIN = NULL_PTR;
    }
    if(curSession->currentSlot->soPIN != NULL_PTR) {
      free(curSession->currentSlot->soPIN);
      curSession->currentSlot->soPIN = NULL_PTR;
    }
  }

  // Remove the session objects created by this session
  destroySessObj(sessions[sessID]);

  // Close the current session;
  delete sessions[sessID];
  sessions[sessID] = NULL_PTR;
  openSessions--;

  DEBUG_MSG("C_CloseSession", "OK");
  return CKR_OK;
}

// Closes all the sessions.

CK_RV SoftHSMInternal::closeAllSessions(CK_SLOT_ID slotID) {
  SoftSlot *currentSlot = slots->getSlot(slotID);

  CHECK_DEBUG_RETURN(currentSlot == NULL_PTR, "C_CloseAllSessions", "The given slotID does not exist",
                     CKR_SLOT_ID_INVALID);

  // Close all sessions on the slot.
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(sessions[i] != NULL_PTR) {
      if(sessions[i]->currentSlot->getSlotID() == slotID) {
        // Remove session objects
        destroySessObj(sessions[i]);

        // Close session
        delete sessions[i];
        sessions[i] = NULL_PTR;
        openSessions--;
      }
    }
  }

  // Log out from the slot
  if(currentSlot->userPIN != NULL_PTR) {
    free(currentSlot->userPIN);
    currentSlot->userPIN = NULL_PTR;
  }
  if(currentSlot->soPIN != NULL_PTR) {
    free(currentSlot->soPIN);
    currentSlot->soPIN = NULL_PTR;
  }

  DEBUG_MSG("C_CloseAllSessions", "OK");
  return CKR_OK;
}

// Destroy all the session objects created by this session

void SoftHSMInternal::destroySessObj(SoftSession *session) {
  SoftObject *currentObject = session->currentSlot->objects;

  // Loop all objects
  while(currentObject->nextObject != NULL_PTR) {
    if(currentObject->isToken == CK_FALSE &&
       currentObject->createdBySession == session) {

      session->db->deleteObject(currentObject->index);
      currentObject->deleteObj(currentObject->index);

      // Need to do a re-check because we moved the object chain
      // since the while statement
      if(currentObject->nextObject == NULL_PTR) {
        break;
      }
    }
    currentObject = currentObject->nextObject;
  }
}

// Return information about the session.

CK_RV SoftHSMInternal::getSessionInfo(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo) {
  SoftSession *session = getSession(hSession);

  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_GetSessionInfo", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);
  CHECK_DEBUG_RETURN(pInfo == NULL_PTR, "C_GetSessionInfo", "pInfo must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);

  pInfo->slotID = session->currentSlot->getSlotID();
  pInfo->state = session->getSessionState();
  pInfo->flags = CKF_SERIAL_SESSION;
  if(session->isReadWrite()) {
    pInfo->flags |= CKF_RW_SESSION;
  }
  pInfo->ulDeviceError = 0;

  DEBUG_MSG("C_GetSessionInfo", "OK");
  return CKR_OK;
}

// Logs the user into the token.

CK_RV SoftHSMInternal::login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen) {
  SoftSession *session = getSession(hSession);

  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_Login", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);
  CHECK_DEBUG_RETURN(pPin == NULL_PTR, "C_Login", "pPin must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);
  CHECK_DEBUG_RETURN(ulPinLen < 4 || ulPinLen > 255, "C_Login", "Incorrent PIN length",
                     CKR_PIN_INCORRECT);

  int logInType = CKU_USER;

  int slotID = session->currentSlot->getSlotID();
  switch(userType) {
    case CKU_SO:
      // Only one user type can be logged in
      CHECK_DEBUG_RETURN(session->currentSlot->userPIN != NULL_PTR, "C_Login", "A normal user is already logged in",
                         CKR_USER_TOO_MANY_TYPES);

      // Check that we have no R/O session with the slot
      for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if(sessions[i] != NULL_PTR) {
          CHECK_DEBUG_RETURN(sessions[i]->currentSlot->getSlotID() == slotID && sessions[i]->isReadWrite() == CK_FALSE, 
                             "C_Login", "No read only session must exist", CKR_SESSION_READ_ONLY_EXISTS);
        }
      }
      logInType = CKU_SO;
      break;
    case CKU_USER:
      // Only one user type can be logged in
      CHECK_DEBUG_RETURN(session->currentSlot->soPIN != NULL_PTR, "C_Login", "A SO is already logged in",
                         CKR_USER_TOO_MANY_TYPES);

      CHECK_DEBUG_RETURN(session->currentSlot->hashedUserPIN == NULL_PTR, "C_Login", "The normal user PIN is not initialized",
                         CKR_USER_PIN_NOT_INITIALIZED);
      break;
    case CKU_CONTEXT_SPECIFIC:
      CHECK_DEBUG_RETURN(session->currentSlot->userPIN == NULL_PTR && session->currentSlot->soPIN == NULL_PTR, "C_Login", 
                         "A previous login must have been performed", CKR_OPERATION_NOT_INITIALIZED);

      if(session->currentSlot->soPIN != NULL_PTR) {
        logInType = CKU_SO;
      }
      break;
    default:
      DEBUG_MSG("C_Login", "The given user type does not exist");
      return CKR_USER_TYPE_INVALID;
      break;
  }

  // Digest the PIN
  // We do not use any salt
  Pipe *digestPIN = new Pipe(new Hash_Filter(new SHA_256), new Hex_Encoder);
  digestPIN->start_msg();
  digestPIN->write(pPin, ulPinLen);
  digestPIN->write(pPin, ulPinLen);
  digestPIN->write(pPin, ulPinLen);
  digestPIN->end_msg();

  // Get the digested PIN
  SecureVector<byte> pinVector = digestPIN->read_all();
  int size = pinVector.size();
  char *tmpPIN = (char *)malloc(size + 1);
  tmpPIN[size] = '\0';
  memcpy(tmpPIN, pinVector.begin(), size);
  delete digestPIN;

  if(logInType == CKU_SO) {
    // Is the PIN incorrect?
    if(strcmp(tmpPIN, session->currentSlot->hashedSOPIN) != 0) {
      free(tmpPIN);

      DEBUG_MSG("C_Login", "The SO PIN is incorrect");
      return CKR_PIN_INCORRECT;
    }

    free(tmpPIN);

    // First login?
    if(session->currentSlot->soPIN == NULL_PTR) {
      // Store the PIN
      session->currentSlot->soPIN = (char *)malloc(ulPinLen + 1);
      session->currentSlot->soPIN[ulPinLen] = '\0';
      memcpy(session->currentSlot->soPIN, pPin, ulPinLen);
    }

    DEBUG_MSG("C_Login", "OK");
    return CKR_OK;
  } else {
    // Is the PIN incorrect?
    if(strcmp(tmpPIN, session->currentSlot->hashedUserPIN) != 0) {
      free(tmpPIN);

      DEBUG_MSG("C_Login", "The user PIN is incorrect");
      return CKR_PIN_INCORRECT;
    }

    free(tmpPIN);

    // First login?
    if(session->currentSlot->userPIN == NULL_PTR) {
      // Store the PIN
      session->currentSlot->userPIN = (char *)malloc(ulPinLen + 1);
      session->currentSlot->userPIN[ulPinLen] = '\0';
      memcpy(session->currentSlot->userPIN, pPin, ulPinLen);

      session->currentSlot->login(session->rng);
    }

    DEBUG_MSG("C_Login", "OK");
    return CKR_OK;
  }
}

// Logs out the user from the token.

CK_RV SoftHSMInternal::logout(CK_SESSION_HANDLE hSession) {
  SoftSession *session = getSession(hSession);

  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_Logout", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);

  if(session->currentSlot->userPIN != NULL_PTR) {
    free(session->currentSlot->userPIN);
    session->currentSlot->userPIN = NULL_PTR;
  }
  if(session->currentSlot->soPIN != NULL_PTR) {
    free(session->currentSlot->soPIN);
    session->currentSlot->soPIN = NULL_PTR;
  }

  DEBUG_MSG("C_Logout", "OK");
  return CKR_OK;
}

// Retrieves the session pointer associated with the session handle.

SoftSession* SoftHSMInternal::getSession(CK_SESSION_HANDLE hSession) {
  if(hSession > MAX_SESSION_COUNT || hSession < 1) {
    return NULL_PTR;
  }

  return sessions[hSession-1];
}

// Retrieves the attributes specified by the template.
// There can be different error states depending on 
// if the given buffer is too small, the attribute is 
// sensitive, or not supported by the object.
// If there is an error, then the most recent one is
// returned.

CK_RV SoftHSMInternal::getAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, 
    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount) {

  SoftSession *session = getSession(hSession);
  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_GetAttributeValue", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);

  SoftObject *object = session->currentSlot->objects->getObject(hObject);
  CHECK_DEBUG_RETURN(object == NULL_PTR, "C_GetAttributeValue", "Can not find the object",
                     CKR_OBJECT_HANDLE_INVALID);

  CK_BBOOL userAuth = userAuthorization(session->getSessionState(), object->isToken, object->isPrivate, 0);
  CHECK_DEBUG_RETURN(userAuth == NULL_PTR, "C_GetAttributeValue", "User is not authorized",
                     CKR_OBJECT_HANDLE_INVALID);

  CHECK_DEBUG_RETURN(pTemplate == NULL_PTR, "C_GetAttributeValue", "pTemplate must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);

  CK_RV result = CKR_OK;
  CK_RV objectResult = CKR_OK;

  for(CK_ULONG i = 0; i < ulCount; i++) {
    objectResult = object->getAttribute(&pTemplate[i]);
    if(objectResult != CKR_OK) {
      result = objectResult;
    }
  }

  DEBUG_MSG("C_GetAttributeValue", "Returning");
  return result;
}

// Set the attributes according to the template.

CK_RV SoftHSMInternal::setAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount) {
  SoftSession *session = getSession(hSession);
  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_SetAttributeValue", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);

  SoftObject *object = session->currentSlot->objects->getObject(hObject);
  CHECK_DEBUG_RETURN(object == NULL_PTR, "C_SetAttributeValue", "Can not find the object",
                     CKR_OBJECT_HANDLE_INVALID);

  CK_BBOOL userAuth = userAuthorization(session->getSessionState(), object->isToken, object->isPrivate, 1);
  CHECK_DEBUG_RETURN(userAuth == NULL_PTR, "C_SetAttributeValue", "User is not authorized",
                     CKR_OBJECT_HANDLE_INVALID);

  CHECK_DEBUG_RETURN(pTemplate == NULL_PTR, "C_SetAttributeValue", "pTemplate must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);

  CK_RV result = CKR_OK;
  CK_RV objectResult = CKR_OK;

  // Loop through all the attributes in the template
  for(CK_ULONG i = 0; i < ulCount; i++) {
    objectResult = object->setAttribute(&pTemplate[i], session->db);
    if(objectResult != CKR_OK) {
      result = objectResult;
    }
  }

  DEBUG_MSG("C_SetAttributeValue", "Returning");
  return result;
}

// Initialize the search for objects.
// The template specifies the search pattern.

CK_RV SoftHSMInternal::findObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount) {
  SoftSession *session = getSession(hSession);
  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_FindObjectsInit", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);

  CHECK_DEBUG_RETURN(session->findInitialized, "C_FindObjectsInit", "Find is already initialized",
                     CKR_OPERATION_ACTIVE);
  CHECK_DEBUG_RETURN(pTemplate == NULL_PTR && ulCount > 0, "C_FindObjectsInit", "pTemplate must not be a NULL_PTR",
                     CKR_ARGUMENTS_BAD);

  if(session->findAnchor != NULL_PTR) {
    delete session->findAnchor;
  }

  // Creates the search result chain.
  session->findAnchor = new SoftFind();
  session->findCurrent = session->findAnchor;

  SoftObject *currentObject = session->currentSlot->objects;

  // Check with all objects.
  while(currentObject->nextObject != NULL_PTR) {
    CK_BBOOL userAuth = userAuthorization(session->getSessionState(), currentObject->isToken, currentObject->isPrivate, 0);

    if(userAuth == CK_TRUE) {
      CK_BBOOL findObject = CK_TRUE;

      // See if the object match all attributes.
      for(CK_ULONG j = 0; j < ulCount; j++) {
        if(currentObject->matchAttribute(&pTemplate[j]) == CK_FALSE) {
          findObject = CK_FALSE;
        }
      }

      // Add the handle to the search results if the object matched the attributes.
      if(findObject == CK_TRUE) {
        session->findAnchor->addFind(currentObject->index);
      }
    }

    // Iterate
    currentObject = currentObject->nextObject;
  }

  session->findInitialized = true;

  DEBUG_MSG("C_FindObjectsInit", "OK");
  return CKR_OK;
}

// Destroys the object.

CK_RV SoftHSMInternal::destroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject) {
  SoftSession *session = getSession(hSession);
  CHECK_DEBUG_RETURN(session == NULL_PTR, "C_DestroyObject", "Can not find the session",
                     CKR_SESSION_HANDLE_INVALID);

  SoftObject *object = session->currentSlot->objects->getObject(hObject);
  CHECK_DEBUG_RETURN(object == NULL_PTR, "C_DestroyObject", "Can not find the object",
                     CKR_OBJECT_HANDLE_INVALID);

  CK_BBOOL userAuth = userAuthorization(session->getSessionState(), object->isToken, object->isPrivate, 1);
  CHECK_DEBUG_RETURN(userAuth == NULL_PTR, "C_DestroyObject", "User is not authorized",
                     CKR_OBJECT_HANDLE_INVALID);

  // Remove the key from the sessions' key cache
  for(int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(sessions[i] != NULL_PTR) {
      sessions[i]->keyStore->removeKey(hObject);
    }
  }

  // Delete the object from the database
  session->db->deleteObject(hObject);

  // Delete the object from the internal state
  CK_RV result = session->currentSlot->objects->deleteObj(hObject);

  INFO_MSG("C_DestroyObject", "An object has been destroyed");
  DEBUG_MSG("C_DestroyObject", "Returning");
  return result;
}

// Wrapper for the mutex function.

CK_RV SoftHSMInternal::createMutex(CK_VOID_PTR_PTR newMutex) {
  if(!usesThreading) {
    return CKR_OK;
  }

  // Calls the real mutex function via its function pointer.
  return createMutexFunc(newMutex);
}

// Wrapper for the mutex function.

CK_RV SoftHSMInternal::destroyMutex(CK_VOID_PTR mutex) {
  if(!usesThreading) {
    return CKR_OK;
  }

  // Calls the real mutex function via its function pointer.
  return destroyMutexFunc(mutex);
}

// Wrapper for the mutex function.

CK_RV SoftHSMInternal::lockMutex() {
  if(!usesThreading) {
    return CKR_OK;
  }

  // Calls the real mutex function via its function pointer.
  return lockMutexFunc(pHSMMutex);
}

// Wrapper for the mutex function.

CK_RV SoftHSMInternal::unlockMutex() {
  if(!usesThreading) {
    return CKR_OK;
  }

  // Calls the real mutex function via its function pointer.
  return unlockMutexFunc(pHSMMutex);
}
