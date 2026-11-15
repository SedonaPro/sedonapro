#include <stdlib.h>
#include <stdbool.h>

#include "expression.h"

static SExpression *allocateExpression()
{
	SExpression *b = (SExpression *)malloc(sizeof(SExpression));
	if (b == NULL) return NULL;
	b->type = eVALUE;
	b->value = NULL;
	b->freeValue = false;
	b->left = NULL;
	b->right = NULL;
	return b;
}

SExpression *createNumber(double value)
{
	SExpression *b = allocateExpression();
	if (b == NULL) return NULL;
	b->type = eVALUE;
	b->value = malloc(sizeof(value));
	if (!b->value) {
		deleteExpression(b);
		return NULL;
	}
	b->freeValue = true;
	*(double*)b->value = value;
	return b;
}

SExpression *createIntNumber(int value, int type)
{
	SExpression *b = allocateExpression();
	if (b == NULL) return NULL;
	b->type = type;
	b->value = malloc(sizeof(value));
	if (!b->value) {
		deleteExpression(b);
		return NULL;
	}
	b->freeValue = true;
	*(int*)b->value = value;
	return b;
}


SExpression *createVariable(void *var, int type)
{
	SExpression *b = allocateExpression();
	if (b == NULL) return NULL;
	b->type = type;
	b->value = var;
	b->freeValue = false;
	return b;
}


SExpression *createOperation(EOperationType type, SExpression *left, SExpression *right)
{
	SExpression *b = allocateExpression();
	if (b == NULL) return NULL;
	b->type = type;
	b->left = left;
	b->right = right;
	return b;
}

void deleteExpression(SExpression *b)
{
	if (b == NULL) return;
	deleteExpression(b->left);
	deleteExpression(b->right);
	if (b->value && b->freeValue)
		free(b->value);
	free(b);
}
