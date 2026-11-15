/*
 * expression.h
 * Definition of the structure used to build the syntax tree.
 */
#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

#include <stdbool.h>

/**
 * @brief Bool values
 */
enum boolValueType {
	boolFalse,
	boolTrue,
	boolNull,
	boolUnknown,
};

/**
 * @brief The operation type
 */
typedef enum tagEOperationType {
	/* bool operations */
	eVARBool,
	eVALUEBool,
	eNOT,
	eOR,
	eAND,
	eXOR,

	/* double operations */
	eVAR,
	eVALUE,
	eMULTIPLY,
	eDIV,
	eMOD,
	ePLUS,
	eSUB,
	eFLOOR,
	eCEIL,
	eABS,
	eCOS,
	eSIN,
	eTAN,
	eSINH,
	eCOSH,
	eTANH,
	eSQRT,
	eCBRT,
	eLOG2,
	eLOG10,
	eLN,
	eEXP,
	ePOW,
} EOperationType;

/**
 * @brief The expression structure
 */
typedef struct tagSExpression{
	EOperationType type;           ///< type of operation
	bool freeValue;
	void* value;                   ///< valid only when type is eVALUE
	struct tagSExpression *left;   ///< left side of the tree
	struct tagSExpression *right;  ///< right side of the tree
} SExpression;

/**
 * @brief It creates an identifier
 * @param value The number value
 * @return The expression or NULL in case of no memory
 */
SExpression *createNumber(double value);


/**
 * @brief It creates an identifier
 * @param value The number value
 * @return The expression or NULL in case of no memory
 */
SExpression *createIntNumber(int value, int type);


/**
 * @brief It creates an identifier for a double variable
 * @param value The pointer to variable with a value
 * @param type The type of variable (eVALUE or eVALUEBool)
 * @return The expression or NULL in case of no memory
 */
SExpression *createVariable(void *var, int type);


/**
 * @brief It creates an operation
 * @param type The operation type
 * @param left The left operand
 * @param right The right operand
 * @return The expression or NULL in case of no memory
 */
SExpression *createOperation(EOperationType type,
			SExpression *left, SExpression *right);

/**
 * @brief Deletes a expression
 * @param b The expression
 */
void deleteExpression(SExpression *b);

#endif // __EXPRESSION_H__
