#include <libxml2/libxml/parser.h>
#include <stdio.h>

xmlNode readxml(char * filepath)
{
    xmlDoc         *document;
    xmlNode        *root, *first_child, *node;

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
    return 0;
}
