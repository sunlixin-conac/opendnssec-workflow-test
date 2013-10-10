#include "xml_parser.h"
#include <stdio.h>

int readxml(char * filepath)
{
  /*  xmlDoc         *document;
    xmlNode        *first_child, *node;

    if (filepath== NULL) {
  
    	printf("%s", "No XML file!");
        return 1;
    }
   

    document = xmlReadFile(filepath, NULL, 0);
    root = xmlDocGetRootElement(document);
    /*fprintf(stdout, "Root is <%s> (%i)\n", root->name, root->type);
    
    first_child = root->children; root->content
    for (node = first_child; node; node = node->next) {
    	fprintf(stdout, "\t Child is <%s> (%i)\n", node->name, node->type);
    }
 */
    
     
    xmlTextReaderPtr reader;
 
 
    reader = xmlReaderForFile(filepath, NULL, 0);
 
 
    const char *temp;
 
 
    int i;
 
 
    while(xmlTextReaderRead(reader)) {
        switch(xmlTextReaderNodeType(reader)) {
            case XML_READER_TYPE_ELEMENT:
 
 
            for(i = 0 ; i < xmlTextReaderDepth(reader) ; i++)
                printf("\t");
 
 
            temp = (char *)xmlTextReaderConstName(reader);
 
 
            printf("Element: %s", temp);
 
 
            while(xmlTextReaderMoveToNextAttribute(reader)) {
                temp = (char *)xmlTextReaderConstName(reader);
                printf("  %s", temp);
 
 
                temp = (char *)xmlTextReaderConstValue(reader);
                printf("=\"%s\"", temp);
            }
 
 
            xmlTextReaderMoveToElement(reader);
 
 
            printf("\n");
 
 
            continue;
 
 
        case XML_READER_TYPE_TEXT:
            temp = (char *)xmlTextReaderConstValue(reader);
 
 
            for(i = 0 ; i < xmlTextReaderDepth(reader) ; i++)
                printf("\t");
 
 
            printf("\t%s", temp);
 
 
            printf("\n");
 
 
            continue;
        }
    }
 
 
    xmlFreeTextReader(reader);
    xmlCleanupParser();
    
    return 0;
}
