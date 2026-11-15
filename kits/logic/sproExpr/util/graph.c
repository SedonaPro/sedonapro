#include "expression.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>

static void write_tree(SExpression *e, FILE *dotfile, int node);

/* -- start graphviz parse tree code --
 *
 * write_graphviz(expression) creates the file tree.dot
 * which can be used to generate an image of the parse tree
 * using the graphiz.org "dot" command, i.e.

 dot -Tpng < tree.dot > tree.png
*/

static char* bool_to_string(int val)
{
	static char *str_vals[] = {
		"False",
		"True",
		"NULL",
		"UNKNOWN"
	};
	if ((val > boolNull) || (val < boolFalse))
		return str_vals[boolUnknown];
	return str_vals[val];
}


/* return char* description of parse tree node, e.g. '+', '-', '3', etc */
static char* node_name(SExpression *e)
{
	const int len = 20;
	char* name = (char *)malloc(len);
	switch (e->type){
	case eVARBool:
		snprintf(name, len, "var: %s", bool_to_string(*(int*)e->value));
		break;
	case eVALUEBool:
		snprintf(name, len, "%s", bool_to_string(*(int*)e->value));
		break;
	case eVAR:
		snprintf(name, len, "var: %f", *(double*)e->value);
		break;
	case eVALUE:
		snprintf(name, len, "%f", *(double*)e->value);
		break;
	case eNOT:
		snprintf(name, len, "NOT");
		break;
	case eAND:
		snprintf(name, len, "AND");
		break;
	case eOR:
		snprintf(name, len, "OR");
		break;
	case eXOR:
		snprintf(name, len, "XOR");
		break;
	case eMULTIPLY:
		snprintf(name, len, "*");
		break;
	case eDIV:
		snprintf(name, len, "/");
		break;
	case eMOD:
		snprintf(name, len, "%");
		break;
	case ePLUS:
		snprintf(name, len, "+");
		break;
	case eSUB:
		snprintf(name, len, "-");
		break;
	case eFLOOR:
		snprintf(name, len, "FLOOR");
		break;
	case eCEIL:
		snprintf(name, len, "CEIL");
		break;
	case eCOS:
		snprintf(name, len, "COS");
		break;
	case eSIN:
		snprintf(name, len, "SIN");
		break;
	case eTAN:
		snprintf(name, len, "TAN");
		break;
	case eSINH:
		snprintf(name, len, "SINH");
		break;
	case eCOSH:
		snprintf(name, len, "COSH");
		break;
	case eTANH:
		snprintf(name, len, "TANH");
		break;
	case eSQRT:
		snprintf(name, len, "SQRT");
		break;
	case eCBRT:
		snprintf(name, len, "CBRT");
		break;
	case eLOG2:
		snprintf(name, len, "LOG2");
		break;
	case eLOG10:
		snprintf(name, len, "LOG10");
		break;
	case eLN:
		snprintf(name, len, "LN");
		break;
	case eEXP:
		snprintf(name, len, "EXP");
		break;
	case ePOW:
		snprintf(name, len, "^");
		break;
	default:  // shouldn't get here
		snprintf(name, len, "???");
	}
	return name;
}



static void write_node(SExpression *e, FILE *dotfile, int node, int parent)
{
	fprintf(dotfile, " %i -> %i;\n", parent, node);
	write_tree(e, dotfile, node);
}

static void write_tree(SExpression *e, FILE *dotfile, int node)
{
	fprintf(dotfile, " %i [label=\"%s\"];\n", node, node_name(e));
	if (e->left != NULL) write_node(e->left, dotfile, 2*node, node);
	if (e->right != NULL) write_node(e->right, dotfile, 2*node+1, node);
}

void write_graphviz(SExpression *e, char *filename)
{
	FILE *dotfile = fopen(filename, "w");
	fprintf(dotfile, "digraph tree {\n");
	write_tree(e, dotfile, 1);
	fprintf(dotfile, "}\n");
	fclose(dotfile);
}
