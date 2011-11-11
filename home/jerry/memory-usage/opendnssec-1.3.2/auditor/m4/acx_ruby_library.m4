# $Id: acx_ruby_library.m4 2718 2010-01-26 11:47:05Z jakob $

AC_DEFUN([ACX_RUBY_LIBRARY],[

	for library in $1;
	do
		AC_MSG_CHECKING([for ruby library $library])

                if ! $RUBY -r$library -e "" > /dev/null 2>&1; then
			AC_MSG_RESULT([not found])
			AC_MSG_ERROR([Ruby library '$library' not found])
		else
			AC_MSG_RESULT([ok])
		fi
	done

])
