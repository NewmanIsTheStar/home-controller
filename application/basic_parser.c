/******************************************************************************
File:       basic_parser.c
Description:Parsing functions for SNMP BASIC.
            Includes support for integer, floating point and string expressions.
******************************************************************************/

/*Include Files*/
#include "setjmp.h"
#include "math.h"
#include "ctype.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "basic.h"


/*External Variables*/
extern char bScriptFileActive;                 //indicates a script is running
extern tsCommand asCommandTable[];             //table that defines all BASIC commands
extern int bClearInkey;                        //flag used to indicate script has read keystroke

const char program_terminated[] = "END\r\n";

/*Private Prototypes*/
extern tsBasicContext *psContext;
void eval_AdditionAndSubtraction(int *piAnswer, double *pfAnswer);
void eval_MultiplicationAndDivision(int *piAnswer, double *pfAnswer), eval_Exponent(int *piAnswer, double *pfAnswer);
void eval_UnaryPlusAndMinus(int *piAnswer, double *pfAnswer), eval_ParenthesizedExpressionOrFunction(int *piAnswer, double *pfAnswer);
void atom(int *piAnswer, double *pfAnswer);
int look_up(char *s);
int isdelim(char c), iswhite(char c);
int find_command(char *acString);
int find_function(char *acString);
int find_logic(char *acString);
void eval_function(int *piAnswer, double *pfAnswer);
void eval_bitwise(int *piAnswer, double *pfAnswer);
void eval_bitshift(int *piAnswer, double *pfAnswer);
void eval_not(int *piAnswer, double *pfAnswer);
void eval_relational(int *piAnswer, double *pfAnswer);


/*BASIC function lookup table*/
tsFunction asFunctionTable[] =
{
    /*Function name must be lowercase in this table.*/   
    "acos",             ACOS,
    "asin",             ASIN,
    "atan",             ATAN,
    "cos",              COS,
    "cosh",             COSH,
    "ln",               LN,
    "sin",              SIN,
    "sinh",             SINH,
    "sqrt",             SQRT,
    "tan",              TAN,
    "tanh",             TANH,

    "chr$",             CHR$,
    "len$",             LEN$,
    "asc",              ASC,
    
    /*Insert new functions above this line*/
    "",                 END, 
};


/*BASIC logic operator lookup table*/
tsFunction asLogicTable[] =
{
    /*Function name must be lowercase in this table.*/   
    "and",              AND,
    "or",               OR,
    "not",              NOT,
 
    /*Insert new functions above this line*/
    "",                 END, 
};

/*****************************************************************
FUNCTION    : eval_StringLogic
DESCRIPTION : Evaluates string expression and places result in
            : user-provided variable
INPUTS      : pcResult - pointer to buffer to store result
            : nResultLen - length of result buffer
OUTPUTS     : Writes into buffer pointed to by pcResult
RETURNS     :
******************************************************************/
int eval_StringLogic(void)
{
    int nMatch = 0;
    char cOperator;
    char acStringOne[3000];
    char acStringTwo[3000];
    int nCompare;

    /*Get first string concatentation*/
    eval_StringExpression(acStringOne, sizeof(acStringOne));

    /*Get operator*/
    get_token();

    /* Evaluate the operator */
    if (!strchr( "=<>", *psContext->acToken))
    {
        syntax_error(SYNTAX); /* not a legal operator */
    }

    /*Operator is first character of token*/
    cOperator = psContext->acToken[0];

    /*Change BASIC not equals operator to a token with unique first character*/
    if ( strcmp( psContext->acToken, "<>" ) == 0 )
        cOperator = '!';
    if ( strcmp( psContext->acToken, ">=" ) == 0 )
        cOperator = ']';
    if ( strcmp( psContext->acToken, "<=" ) == 0 )
        cOperator = '[';


    /*Get second string concatenation*/
    eval_StringExpression(acStringTwo, sizeof(acStringTwo));


    /*Do compare*/
    nCompare = strcmp(acStringOne, acStringTwo);


    switch(cOperator)
    {
    case '=':
      if (nCompare == 0) nMatch = 1;
      break;
    case '!':
      if (nCompare != 0) nMatch = 1;
      break;
    case '<':
      if (nCompare < 0) nMatch = 1;
      break;
    case '>':
      if (nCompare > 0) nMatch = 1;
      break;
    case '[':
      if (nCompare <= 0) nMatch = 1;
      break;
    case ']':
      if (nCompare >= 0) nMatch = 1;
      break;
    default:
      syntax_error(SYNTAX);
    }

    return(nMatch);
}



/*****************************************************************
FUNCTION    : eval_StringExpression
DESCRIPTION : Evaluates string expression and places result in
            : user-provided variable
INPUTS      : pcResult - pointer to buffer to store result
            : nResultLen - length of result buffer
OUTPUTS     : Writes into buffer pointed to by pcResult
RETURNS     :
******************************************************************/
void eval_StringExpression(char *pcResult, int nResultLen)
{
    eval_StringLine(pcResult, nResultLen, 0);
}


/*****************************************************************
FUNCTION    : eval_StringLine
DESCRIPTION : Evaluates string expression and places result in
            : user-provided variable.  Optionally prints 
              carriage return unless suppressed by ; or ,
INPUTS      : pcResult - pointer to buffer to store result
            : nResultLen - length of result buffer
OUTPUTS     : Writes into buffer pointed to by pcResult
RETURNS     :
******************************************************************/
void eval_StringLine(char *pcResult, int nResultLen, int iProcessEol)
{
    int nTokenType;
    int nStrVarIdx;
    int nLength;
    char *pcActivePos;
    int iTemp = 0;
    int spaces;
    int iLineFeed = 1;
    int iAnswer;
    double fAnswer;
    teToken eToken;
    int iEndOfString;
	char acTemp[3000];

    pcActivePos = pcResult;
    *pcActivePos = '\0';

    iEndOfString = 0;

    do
    {
        nTokenType = get_token();
        eToken = psContext->eToken;

        switch(nTokenType) 
        {
          case STRINGVARIABLE:
              pcActivePos += string_atom(pcActivePos, nResultLen - (pcActivePos - pcResult)); 
              iLineFeed = 1;
              break;

          case NUMBER:
          case FLOATVARIABLE:
          case INTEGERVARIABLE:
			putback();
			eval_NumericExpression(&iAnswer, &fAnswer);

            /*avoid printing decimal point if whole number*/
            if ((fAnswer - (double)iAnswer) == 0)
            {
                pcActivePos += sprintf(pcActivePos, "%d", iAnswer);
            }
            else
            {
                pcActivePos += sprintf(pcActivePos, "%g", fAnswer);
            }
            iLineFeed = 1;
            break;
            
          case QUOTE:
              nLength = strlen( psContext->acToken );
              if ( nLength >= ( nResultLen - (pcActivePos - pcResult) ) )
              {
                  // out of memory
                  syntax_error( OUT_OF_MEM );
                  return;
              }
              else
              {
                  strcpy( pcActivePos, psContext->acToken );
                  pcActivePos += nLength;
              }
              iLineFeed = 1;
              break;

          case FUNCTION:              
              switch (psContext->eToken)
              {
                    case CHR$:
                        get_Bracket('(');
                        eval_IntegerExpression(&iTemp);
                        pcActivePos += sprintf(pcActivePos, "%c", (char)iTemp);
                        get_Bracket(')');
                        break;
						
                    case LEN$:
                        get_Bracket('(');
                        eval_StringExpression(acTemp, sizeof(acTemp));
						iTemp = strlen(acTemp);
                        pcActivePos += sprintf(pcActivePos, "%d", (char)iTemp);
                        get_Bracket(')');
                        break;

                    case ASC:
                        get_Bracket('(');
                        eval_StringExpression(acTemp, sizeof(acTemp));
						iTemp = acTemp[0];
                        pcActivePos += sprintf(pcActivePos, "%d", (char)iTemp);
                        get_Bracket(')');
                        break;
						
                    default:
                        printf("Unexpected function call in string expression\n");
                        syntax_error(SYNTAX);
                        break;
              }
              iLineFeed = 1;
              break;

          case USERFUNCTION:              
              basic_Function();

              nStrVarIdx = find_Variable("returnvalue$", STRINGVARIABLE);
              nLength = strlen(get_StringVariable("returnvalue$"));
              if (nLength >= (nResultLen - (pcActivePos - pcResult)))
              {
                  // out of memory
                  syntax_error( OUT_OF_MEM );
                  return;
              }
              else
              {
                  strcpy(pcActivePos, psContext->asStringVariables[nStrVarIdx].acValue);
                  pcActivePos += nLength;
               }
              iLineFeed = 1;
              break;
          
          case DELIMITER:		      
		      if(*psContext->acToken == ',')
		      {
                if (iProcessEol)
                {  
			      // compute number of spaces to move to next tab
			      spaces = 8 - ((pcActivePos-pcResult) % 8);           
			      while(spaces)
                  {
			         pcActivePos += sprintf(pcActivePos, " ");
			         spaces--;
                  }
                }
                else
                {
                    iEndOfString = 1;
                }
		      }
              else if ((*psContext->acToken == ')')      ||
                       (*psContext->acToken == '>')      ||
                       (*psContext->acToken == '<')      ||
                       (*psContext->acToken == '=')      ||
                       (psContext->eToken   == EOL)      ||
                       (psContext->eToken   == FINISHED))
              {
                  iEndOfString = 1;
              }
              else if((*psContext->acToken != ';')       &&
                      (*psContext->acToken != '+'))
              {
                  syntax_error(SYNTAX);
              }


              break;

          default:
              iEndOfString = 1;
              break;

        } // end switch token type

        if((*psContext->acToken==',') || (*psContext->acToken==';'))
        {
            iLineFeed = 0;
        }

    } while (!iEndOfString);
    

    if(iProcessEol && iLineFeed)
    {
        pcActivePos += sprintf(pcActivePos, "\n");
    }

    // last token fetched was not a part of the string, so put it back
    putback();

    *pcActivePos = '\0';

} // end eval_StringExpression


/*****************************************************************
FUNCTION    : string_atom
DESCRIPTION : Find string value of number or variable
INPUTS      : answer - pointer to string to store result
OUTPUTS     : string primitive
RETURNS     :
******************************************************************/
int string_atom(char *pcAnswer, int iLength)
{
    float fTemp = 0;
    teTokenType eVariableType;
    int iIndexArray[MAX_ARRAY_DIM];
    int iIndex;
    char acName[256];
    char acAnswer[3000];
    int iAnswer;
    double fAnswer;

    eVariableType = psContext->eTokenType;
    strncpy(acName, psContext->acToken, sizeof(acName));
    acName[sizeof(acName)-1] = 0;
   
    // check if this is an array variable
    if (psContext->pcProgramCounter[0] == '(')
    {
        get_token(); // get the opening bracket

        // zero index array
        for (iIndex=0; iIndex < MAX_ARRAY_DIM; iIndex++)
        {
            iIndexArray[iIndex] = 0;
        }

        // get array index values
        for (iIndex=0; iIndex < MAX_ARRAY_DIM; iIndex++)
        {
            get_token();
            if (psContext->acToken[0] == ',') get_token();
            if (psContext->acToken[0] == ')') break;

            putback();

            eval_IntegerExpression(&iIndexArray[iIndex]);            
        }

        // get array value
        switch(eVariableType)
        {
            case INTEGERVARIABLE:
                iAnswer = get_IntegerArrayVariable(acName, iIndexArray);
                fAnswer = (double)get_IntegerArrayVariable(acName, iIndexArray);
                sprintf(acAnswer, "%d", iAnswer);
                break;

            case FLOATVARIABLE:
                fAnswer = get_FloatArrayVariable(acName, iIndexArray);
                iAnswer = (int)get_FloatArrayVariable(acName, iIndexArray);

                /*avoid printing decimal point if whole number*/
                if ((fAnswer - (double)iAnswer) == 0)
                {
                    sprintf(acAnswer, "%d", iAnswer);
                }
                else
                {
                    sprintf(acAnswer, "%g", fAnswer);
                }
                break;
            
            case STRINGVARIABLE:
                strcpy(acAnswer, get_StringArrayVariable(acName, iIndexArray));
                break;

            default:
                syntax_error(SYNTAX);
                break;
        }
    }
    else
    {       
        // get non-array value
        switch(eVariableType)
        {
            case INTEGERVARIABLE:
                iAnswer = get_IntegerVariable(psContext->acToken);
                fAnswer = (double)get_IntegerVariable(psContext->acToken);
                sprintf(acAnswer, "%d", iAnswer);
                break;

            case FLOATVARIABLE:
                fAnswer = get_FloatVariable(psContext->acToken);
                iAnswer = (int)get_FloatVariable(psContext->acToken);

                /*avoid printing decimal point if whole number*/
                if ((fAnswer - (double)iAnswer) == 0)
                {
                    sprintf(acAnswer, "%d", iAnswer);
                }
                else
                {
                    sprintf(acAnswer, "%g", fAnswer);
                }
                break;

            case STRINGVARIABLE:
                strcpy(acAnswer, get_StringVariable(acName));
                break;

            case NUMBER:
                iAnswer = atoi(psContext->acToken);
                sscanf(psContext->acToken, "%f", &fTemp);;
                fAnswer = (double)fTemp;
                break;

            default:
                syntax_error(SYNTAX);
                break;
        }
    }

    strncpy(pcAnswer, acAnswer, iLength);
    pcAnswer[iLength-1] = 0;

    return(strlen(acAnswer));
}

/*****************************************************************
FUNCTION    : eval_NumericExpression
DESCRIPTION : Evaluates a numeric expression and places result in
            : user-provided variable
INPUTS      : piAnswer - pointer to int to store result
              pfAnswer - pointer to double to store result
OUTPUTS     : Writes into int and double pointed to by parameters
******************************************************************/
void eval_NumericExpression(int *piAnswer, double *pfAnswer)
{
    int iTemp;
    double fTemp;

    /*Caller may pass NULL pointer for unwanted result*/
    if (!piAnswer) piAnswer = &iTemp;
    if (!pfAnswer) pfAnswer = &fTemp;

    get_token();
    if(!*psContext->acToken)
    {
        syntax_error(NO_EXP);
        return;
    }

    eval_bitwise(piAnswer, pfAnswer);
    putback(); /* return last token read to input stream */
}


/*****************************************************************
FUNCTION    : eval_IntegerExpression
DESCRIPTION : Evaluates a numeric expression and places result in
            : user-provided variable
INPUTS      : piAnswer - pointer to int to store result
              pfAnswer - pointer to double to store result
OUTPUTS     : Writes into int and double pointed to by parameters
******************************************************************/
void eval_IntegerExpression(int *piAnswer)
{
    double fTemp;

    get_token();
    if(!*psContext->acToken)
    {
        syntax_error(NO_EXP);
        return;
    }

    eval_bitwise(piAnswer, &fTemp);
    putback(); /* return last token read to input stream */
}

/*****************************************************************
FUNCTION    : eval_FloatExpression
DESCRIPTION : Evaluates a numeric expression and places result in
            : user-provided variable
INPUTS      : piAnswer - pointer to int to store result
              pfAnswer - pointer to double to store result
OUTPUTS     : Writes into int and double pointed to by parameters
******************************************************************/
void eval_FloatExpression(double *pfAnswer)
{
    int iTemp;


    get_token();
    if(!*psContext->acToken)
    {
        syntax_error(NO_EXP);
        return;
    }

    eval_bitwise(&iTemp, pfAnswer);
    putback(); /* return last token read to input stream */
}

/*****************************************************************
FUNCTION    : eval_bitwise
DESCRIPTION : Evaluate bitwise AND and OR
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_bitwise(int *piAnswer, double *pfAnswer)
{
    char  op;
    int iTemp;
    double fTemp;

    eval_bitshift(piAnswer, pfAnswer);

    op = *psContext->acToken;
    while((op == '&') || (op == '|') || 
          ((psContext->eTokenType == LOGIC) && ((op == 'a') || (op == 'o'))))
    {
        get_token();
        eval_bitshift(&iTemp, &fTemp);
    
        switch(op)
        {
        case '&' :
        case 'a' :
            *piAnswer = (*piAnswer & iTemp);
            *pfAnswer = (double)(*piAnswer);
            break;

        case '|':
        case 'o':
            *piAnswer = (*piAnswer | iTemp);
            *pfAnswer = (double)(*piAnswer);
            break;
        }

        op = *psContext->acToken;
    }
}

/*****************************************************************
FUNCTION    : eval_bitshift
DESCRIPTION : Evaluate bit shift left or right
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_bitshift(int *piAnswer, double *pfAnswer)
{
    char  op;
    int iTemp;
    double fTemp;

    eval_relational(piAnswer, pfAnswer);
    while((strcmp(psContext->acToken, ">>") == 0) || 
          (strcmp(psContext->acToken, "<<") == 0))
    {
        /*Remember first character of operator.  This is sufficient ID.*/
        op = *psContext->acToken;
        
        get_token();
        eval_relational(&iTemp, &fTemp);
    
        switch(op)
        {
        case '>' :
            *piAnswer = (*piAnswer >> iTemp);
            *pfAnswer = (double)(*piAnswer);
            break;

        case '<':
            *piAnswer = (*piAnswer << iTemp);
            *pfAnswer = (double)(*piAnswer);
            break;
        }
    }
}


/***************************************************************************
Function    :  eval_relational
Description :  Support IF statement.
Returns     :  Nothing
***************************************************************************/
void eval_relational(int *piAnswer, double *pfAnswer)
{
    char  op;
    int iTemp;
    double fTemp;
	double fCompare;

    if ((psContext->eTokenType == STRINGVARIABLE) ||
        (psContext->eTokenType == QUOTE))
    {
        putback();
        *piAnswer = eval_StringLogic();
        *pfAnswer = (double)(*piAnswer);
        get_token();
    }
    else
    {
        eval_AdditionAndSubtraction(piAnswer, pfAnswer);
        while((strcmp(psContext->acToken, ">") == 0)  || 
              (strcmp(psContext->acToken, "<") == 0)  || 
              (strcmp(psContext->acToken, "<>") == 0) ||
              (strcmp(psContext->acToken, ">=") == 0) || 
              (strcmp(psContext->acToken, "=") == 0)  || 
              (strcmp(psContext->acToken, "<=") == 0))
        {
            /*Remember first character of operator.  This is sufficient ID.*/
            // TEST TEST try get operator up here ---------------
            //get_token();
            op = *psContext->acToken;

		    /*Change relational operator to a token with unique first character*/
		    if ( strcmp( psContext->acToken, "<>" ) == 0 )
			    op = '!';
		    if ( strcmp( psContext->acToken, ">=" ) == 0 )
			    op = ']';
		    if ( strcmp( psContext->acToken, "<=" ) == 0 )
			    op = '[';
		            
            get_token();  //WHY down here?????  --------------------
            eval_AdditionAndSubtraction(&iTemp, &fTemp);

		    fCompare = *pfAnswer - fTemp;  //left minus right
		    *piAnswer = 0;

		    switch(op)
		    {
		    case '=':
              if (fCompare == 0) *piAnswer = 1;
              break;
		    case '!':
              if (fCompare != 0) *piAnswer = 1;
              break;
		    case '<':
              if (fCompare < 0) *piAnswer = 1;
              break;
		    case '>':
              if (fCompare > 0) *piAnswer = 1;
              break;
		    case '[':
              if (fCompare <= 0) *piAnswer = 1;
              break;
		    case ']':
              if (fCompare >= 0) *piAnswer = 1;
              break;
		    default:
              syntax_error(SYNTAX);
              return;
		    }

		    *pfAnswer = (double)(*piAnswer);
    
        }
    }
}


/*****************************************************************
FUNCTION    : eval_AdditionAndSubtraction
DESCRIPTION : Evaluate addition and subtraction
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_AdditionAndSubtraction(int *piAnswer, double *pfAnswer)
{
    char  op;
    int iTemp;
    double fTemp;

    eval_MultiplicationAndDivision(piAnswer, pfAnswer);
    while((op = *psContext->acToken) == '+' || op == '-')
    {
        get_token();
        eval_MultiplicationAndDivision(&iTemp, &fTemp);
    
        switch(op)
        {
        case '-' :
            *piAnswer = *piAnswer - iTemp;
            *pfAnswer = *pfAnswer - fTemp;
            break;

        case '+':
            *piAnswer = *piAnswer + iTemp;
            *pfAnswer = *pfAnswer + fTemp;
            break;
        }
    }
}


/*****************************************************************
FUNCTION    : eval_MultiplicationAndDivision
DESCRIPTION : Evaluate multiplication and division
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_MultiplicationAndDivision(int *piAnswer, double *pfAnswer)
{
    char  op;
    int iTemp;
    double fTemp;

    eval_Exponent(piAnswer, pfAnswer);
    while((op = *psContext->acToken) == '*' || op == '/')
    {
        get_token();
        eval_Exponent(&iTemp, &fTemp);
    
        switch(op)
        {
        case '*' :
            *piAnswer = *piAnswer * iTemp;
            *pfAnswer = *pfAnswer * fTemp;
            break;

        case '/':
            *piAnswer = *piAnswer / iTemp;
            *pfAnswer = *pfAnswer / fTemp;
            break;
        }
    }
}


/*****************************************************************
FUNCTION    : eval_Exponent
DESCRIPTION : Evaluate exponent
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
NOTE        : THIS NEEDS WORK FOR NEGATIVE EXPONENTS
******************************************************************/
void eval_Exponent(int *piAnswer, double *pfAnswer)
{
    int i, t;
    int iTemp;
    double fTemp;

    eval_UnaryPlusAndMinus(piAnswer, pfAnswer);
    if(*psContext->acToken== '^')
    {
        get_token();
        eval_Exponent(&iTemp, &fTemp);
        if(iTemp==0)
        {
            *piAnswer = 1;
            *pfAnswer = 1;
            return;
        }
        
        i = *piAnswer;
        for(t=iTemp-1; t>0;  t--) *piAnswer = (*piAnswer) * i;
        *pfAnswer = (double)(*piAnswer);
    }
}


/*****************************************************************
FUNCTION    : eval_UnaryPlusAndMinus
DESCRIPTION : Evaluate unary + and - and ~
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_UnaryPlusAndMinus(int *piAnswer, double *pfAnswer)
{
    char  op;

    op = 0;
    if(((psContext->eTokenType==DELIMITER) &&
        *psContext->acToken=='+' ||
        *psContext->acToken=='~' ||
        *psContext->acToken=='-') ||
        ((psContext->eTokenType == LOGIC) && (*psContext->acToken=='n' )))
    {
        op = *psContext->acToken;
        get_token();
    }
    eval_ParenthesizedExpressionOrFunction(piAnswer, pfAnswer);
    if(op=='-')
    {
        *piAnswer = -(*piAnswer);
        *pfAnswer = -(*pfAnswer);
    }

    if(op=='~') 
    {
        *piAnswer = ~(*piAnswer);
        *pfAnswer = (double)(*piAnswer);
    }
}


/*****************************************************************
FUNCTION    : eval_ParenthesizedExpressionOrFunction
DESCRIPTION : Evaluate parenthesized expression or function
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_ParenthesizedExpressionOrFunction(int *piAnswer, double *pfAnswer)
{
    if(*psContext->acToken == '(')
    {
        get_token();

        eval_bitwise(piAnswer, pfAnswer);
        if(*psContext->acToken != ')')
        {
            syntax_error(UNBAL_PARENS);
        }

        get_token();
    }
    else if (psContext->eTokenType == FUNCTION)
    {
        eval_function(piAnswer, pfAnswer);
    }
    else if (psContext->eTokenType == USERFUNCTION)
    {
        basic_Function();
        get_token(); 

        *piAnswer = get_IntegerVariable("returnvalue%");
        *pfAnswer = get_FloatVariable("returnvalue");

    }
    else
    {
        atom(piAnswer, pfAnswer);
    }
}

/*****************************************************************
FUNCTION    : eval_function
DESCRIPTION : Evaluate function 
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void eval_function(int *piAnswer, double *pfAnswer)
{
    int iFunction =  0;
	char acTemp[3000];

    iFunction = psContext->eToken;
    

	if (iFunction == LEN$)
	{
		//get_Bracket('(');
		get_token();		
		eval_StringExpression(acTemp, sizeof(acTemp));

		*piAnswer = strlen(acTemp);;
		*pfAnswer = (double)*piAnswer;

		get_Bracket(')');    
		get_token();
    }
	else if (iFunction == ASC)
	{
		//get_Bracket('(');
		get_token();		
		eval_StringExpression(acTemp, sizeof(acTemp));

		*piAnswer = acTemp[0];
		*pfAnswer = (double)*piAnswer;

		get_Bracket(')');    
		get_token();
    }
	else
	{
		if(*psContext->acToken == '(')
		{
			get_token();
			eval_bitwise(piAnswer, pfAnswer);
			if(*psContext->acToken != ')')
			{
				syntax_error(UNBAL_PARENS);
			}
			get_token();
		}
		else
		{
			syntax_error(MISS_PARENS);
		}

		switch(iFunction)
		{
		case ACOS:
			*pfAnswer = acos(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case ASIN:
			*pfAnswer = asin(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case ATAN:
			*pfAnswer = atan(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case COS:
			*pfAnswer = cos(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case COSH:
			*pfAnswer = cosh(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case LN:
			*pfAnswer = log(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case SIN:
			*pfAnswer = sin(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case SINH:
			*pfAnswer = sinh(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case SQRT:
			*pfAnswer = sqrt(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case TAN:
			*pfAnswer = tan(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		case TANH:
			*pfAnswer = tanh(*pfAnswer);
			*piAnswer = (int)((*pfAnswer) * 10000 + 0.5);
			break;

		default: syntax_error(SYNTAX);
		}
	}
}

/*****************************************************************
FUNCTION    : atom
DESCRIPTION : Find value of number or variable
INPUTS      : answer - pointer to integer to store result
OUTPUTS     : Writes into integer pointed to by answer
RETURNS     :
******************************************************************/
void atom(int *piAnswer, double *pfAnswer)
{
    float fTemp = 0;
    teTokenType eVariableType;
    int iIndexArray[MAX_ARRAY_DIM];
    int iIndex;
    char acName[256];

    eVariableType = psContext->eTokenType;
    strncpy(acName, psContext->acToken, sizeof(acName));
    acName[sizeof(acName)-1] = 0;
   
    // check if this is an array variable
    if (psContext->pcProgramCounter[0] == '(')
    {
        get_token(); // get the opening bracket

        // zero index array
        for (iIndex=0; iIndex < MAX_ARRAY_DIM; iIndex++)
        {
            iIndexArray[iIndex] = 0;
        }

        // get array index values
        for (iIndex=0; iIndex < MAX_ARRAY_DIM; iIndex++)
        {
            get_token();
            if (psContext->acToken[0] == ',') get_token();
            if (psContext->acToken[0] == ')') break;

            putback();

            eval_IntegerExpression(&iIndexArray[iIndex]);            
        }

        // get array value
        switch(eVariableType)
        {
            case INTEGERVARIABLE:
                *piAnswer = get_IntegerArrayVariable(acName, iIndexArray);
                *pfAnswer = (double)get_IntegerArrayVariable(acName, iIndexArray);
                get_token();
                break;

            case FLOATVARIABLE:
                *pfAnswer = get_FloatArrayVariable(acName, iIndexArray);
                *piAnswer = (int)get_FloatArrayVariable(acName, iIndexArray);
                get_token();
                break;

            default:
                syntax_error(SYNTAX);
                break;
        }
    }
    else
    {       
        // get non-array value
        switch(eVariableType)
        {
            case INTEGERVARIABLE:
                *piAnswer = get_IntegerVariable(psContext->acToken);
                *pfAnswer = (double)get_IntegerVariable(psContext->acToken);
                get_token();
                break;

            case FLOATVARIABLE:
                *pfAnswer = get_FloatVariable(psContext->acToken);
                *piAnswer = (int)get_FloatVariable(psContext->acToken);
                get_token();
                break;

            case NUMBER:
                *piAnswer = atoi(psContext->acToken);
                sscanf(psContext->acToken, "%f", &fTemp);;
                *pfAnswer = (double)fTemp;
                get_token();
                break;

            default:
                syntax_error(SYNTAX);
                break;
        }
    }

    return;
}


/***************************************************************************
Function    :  syntax_error
Description :  Display an syntax error message.
Returns     :  Nothing
***************************************************************************/
void syntax_error(int error)
{
    char *p, *temp;
    int linecount = 0;
    int i;

    static char *e[]= {
    "Syntax error",                                                /* SYNTAX */
    "Unbalanced parentheses",                                /* UNBAL_PARENS */
    "No expression present",                                       /* NO_EXP */
    "Equals sign expected",                                    /* EQUALS_EXP */
    "Not a variable",                                             /* NOT_VAR */
    "Label table full",                                      /* LAB_TAB_FULL */
    "Duplicate label",                                            /* DUP_LAB */
    "Undefined label",                                          /* UNDEF_LAB */
    "Function table full",                                  /* FUNC_TAB_FULL */
    "Duplicate function",                                        /* DUP_FUNC */
    "Undefined function",                                      /* UNDEF_FUNC */
    "THEN expected",                                             /* THEN_EXP */
    "TO expected",                                                 /* TO_EXP */
    "Too many nested FOR loops",                              /* TOO_MNY_FOR */
    "NEXT without FOR",                                       /* NEXT_WO_FOR */
    "Too many nested WHILE loops",                          /* TOO_MNY_WHILE */
    "WEND without WHILE",                                   /* WHILE_WO_WEND */
    "Too many nested GOSUBs",                               /* TOO_MNY_GOSUB */
    "RETURN without GOSUB",                                  /* RET_WO_GOSUB */
    "Double quotes needed",                                    /* MISS_QUOTE */
    "Missing parentheses",                                    /* MISS_PARENS */
    "Out of memory",                                           /* OUT_OF_MEM */
    "END IF expected",                                           /* NO_ENDIF */
    "Too many ELSE in IF block",                             /* TOO_MNY_ELSE */
    "Unimplemented command",                                /* UNIMPLEMENTED */
    };
    
    // only process the syntax error if the program has not already been terminated
    if (psContext->pcProgramCounter != program_terminated)
    {
        /*Print the error message. A * is used to mark the last cursor position*/
        printf("*\n\n%s", e[error]);

        /*Find line number of error*/
        p = psContext->pcProgram;
        while(p != psContext->pcProgramCounter)
        {        
            p++;
            //if(*p == '\r')  // original for DOS EOL
            if(*p == '\n')    // should work for both DOS and UNIX EOL        
            {
                linecount++;
            }
        }
        
        /*Print file name and line number where error occured*/
        printf(" in %s at line %d\n", psContext->acFileName, linecount);

        /*Display lines with error*/
        temp = p;

        /*Go back one line*/
        for(i=0; (i<160) && (p>psContext->pcProgram) && (*p!='\n'); i++, p--)
        {
        }

        /*Go back two lines if possible*/
        if (p>psContext->pcProgram)
        {
            p--;

            for(; (i<160) && (p>psContext->pcProgram) && (*p!='\n'); i++, p--)
            {
            }
            if (*p!='\n') p++;
        }

        /*Print out the lines*/
        for(; p<=temp; p++) printf("%c", *p);
        printf("\n\n");

        /*Terminate Script File*/
        bScriptFileActive = 0;

        // /*Restore C program to state before script was run*/
        // longjmp(psContext->sEnviroment, 1);  NOT SUITABLE IN FREERTOS environment
        //psContext->pcProgramCounter = psContext->pcProgram + strlen(psContext->pcProgram) - strlen("END\n" - 1;
        psContext->pcProgramCounter = (char *)program_terminated;
    }
}

/***************************************************************************
Function    :  get_token
Description :  Get the next token from the program.
Returns     :  Nothing
***************************************************************************/
int get_token(void)
{
    char *temp;
    int i;

    psContext->eTokenType = 0;
    psContext->eToken = 0;
    temp = psContext->acToken;

    /*Check for End of File or program termination*/
    if((*psContext->pcProgramCounter==0) ||
       (*psContext->pcProgramCounter==-1) ||
       (psContext->pcProgramCounter == program_terminated))
    {
        *psContext->acToken = 0;
        psContext->eToken = FINISHED;
        return(psContext->eTokenType=DELIMITER);
    }

    /*Skip over white space*/
    while(iswhite(*psContext->pcProgramCounter)) ++psContext->pcProgramCounter;

    /*Check for DOS end of line*/
    if(*psContext->pcProgramCounter=='\r')
    {
        i = 0;
        psContext->acToken[i]=*psContext->pcProgramCounter;        // copy \r into acTokem

        ++psContext->pcProgramCounter;
        
        if(*psContext->pcProgramCounter=='\n')
        {
            psContext->acToken[++i]=*psContext->pcProgramCounter;  // copy \n into acTokem

            ++psContext->pcProgramCounter;                         // point to next token
        }
        else
        {
            printf("Carriage return without Newline discovered.  Treating as EOL.\n");
        }

        psContext->acToken[++i]=0;                                 // terminate acToken
        psContext->eToken = EOL;
        
        return (psContext->eTokenType = DELIMITER);
    }

    /*Check for UNIX end of line*/
    if(*psContext->pcProgramCounter=='\n')
    {
        ++psContext->pcProgramCounter; // next token
        psContext->eToken = EOL;
        psContext->acToken[0]='\n';
        psContext->acToken[1]=0;
        return (psContext->eTokenType = DELIMITER);
    }

    /*Check if token is an operator*/
    if(strchr("+-*^/=;(),><|&~", *psContext->pcProgramCounter))
    {
        /*Delimiter*/
        *temp = *psContext->pcProgramCounter;
        psContext->pcProgramCounter++; /* advance to next position */
        temp++;

        /*Grab second half of two character operators: >=, <= <> >> <<*/
        if ((strchr("><", *psContext->acToken)) && (strchr("><=", *psContext->pcProgramCounter)))
        {
            *temp = *psContext->pcProgramCounter;
            psContext->pcProgramCounter++; /* advance to next position */
            temp++;
        }
        *temp = 0;
        return (psContext->eTokenType=DELIMITER);
    }

    /*Check if token is a quoted string*/
    if(*psContext->pcProgramCounter=='"')
    {
        /*quoted string*/
        psContext->pcProgramCounter++;
        while(*psContext->pcProgramCounter!='"'  &&
              *psContext->pcProgramCounter!='\r' &&      // DOS EOL
              *psContext->pcProgramCounter!='\n')        // UNIX EOL
        {
            *temp++ = *psContext->pcProgramCounter++;

            if ((temp - psContext->acToken) >= sizeof(psContext->acToken))
            {
                // too many characters without finding end quote
                syntax_error(MISS_QUOTE);
                break;
            }
        }

        if((*psContext->pcProgramCounter=='\r') ||       // DOS EOL
           (*psContext->pcProgramCounter=='\n'))         // UNIX EOL
        {
            syntax_error(MISS_QUOTE);
        } 

        psContext->pcProgramCounter++;
        *temp = 0;
        
        return(psContext->eTokenType=QUOTE);
    }

    /*Check if token is a literal number*/
    if(isdigit(*psContext->pcProgramCounter))
    {
        /* number */
        while(!isdelim(*psContext->pcProgramCounter))
        {
            *temp++ = *psContext->pcProgramCounter++;

            if ((temp - psContext->acToken) >= sizeof(psContext->acToken))
            {
                // too many characters
                syntax_error(SYNTAX);
                break;
            }            
        }

        *temp = '\0';

        return(psContext->eTokenType = NUMBER);
    }

    /*Check if token is an alphanumeric string*/
    if(isalpha(*psContext->pcProgramCounter))
    {
        /* var or command */
        while(!isdelim(*psContext->pcProgramCounter))
        {
            *temp++ = *psContext->pcProgramCounter++;

            if ((temp - psContext->acToken) >= sizeof(psContext->acToken))
            {
                // too many characters
                syntax_error(SYNTAX);
                break;
            }              
        }
        psContext->eTokenType = STRING;
    }

    *temp = '\0';  // TODO: Should this be inside the closing brace for the last if statement?

    /*Determine what type of string we have found (command, function, variable or label)*/
    if(psContext->eTokenType==STRING)
    {
        char *p;
        
        /*Convert to lower case.  This should be removed once all code that relies on lower case is fixed*/
        p = psContext->acToken;
        while(*p)
        {
            *p = tolower(*p);
            p++;
        }
        
        //printf("STRING TOKEN: %s\n", psContext->acToken);

        /*Find token type*/
        if((psContext->eToken = find_command(psContext->acToken)) != 0)
        {
            /*command*/
            psContext->eTokenType = COMMAND;
        }
        else if ((psContext->eToken = find_function(psContext->acToken)) != 0)
        {
            /*function*/
            psContext->eTokenType = FUNCTION;
        }
        else if ((psContext->eToken = find_logic(psContext->acToken)) != 0)
        {
            /*logic operator*/
            psContext->eTokenType = LOGIC;
        }
        else if (psContext->acToken[strlen(psContext->acToken)-1] == ':')
        {
            /*label*/
            psContext->eTokenType = LABEL;
        }
        else if (find_UserFunction(psContext->acToken))
        {
            /*user function*/
            psContext->eTokenType = USERFUNCTION;
        }        
        else if (psContext->acToken[strlen(psContext->acToken)-1] == '$')
        {
            /*string variable*/
            psContext->eTokenType = STRINGVARIABLE;
        }
        else if (psContext->acToken[strlen(psContext->acToken)-1] == '%')
        {
            /*integer variable*/
            psContext->eTokenType = INTEGERVARIABLE;
        }
        else
        {
            /*floating point variable*/
            psContext->eTokenType = FLOATVARIABLE;
        }
    }

    if ((psContext->eTokenType == 0) && (*psContext->pcProgramCounter!=0))  // Newman added check for end of string
    {
        /*Kludge to prevent lockup when illegal characters are encountered.
        If an unrecognised token is found then move the PC forward one character.
        This prevents a lockup occuring in any code that loops calling get_token
        unitl a particular token is found or the end of file is reached.*/
        psContext->pcProgramCounter++;
    }

    //printf("Token type = %d Token = %s\n", psContext->eTokenType, psContext->acToken);
    return psContext->eTokenType;
}



/***************************************************************************
Function    :  putback
Description :  Return a token to input stream.
Returns     :
***************************************************************************/
void putback(void)
{

    char *t;

    t = psContext->acToken;
    for(; *t; t++) psContext->pcProgramCounter--;

    /*If putting back a quoted string must move back two extra characters for quotes*/
    if (psContext->eTokenType == QUOTE) psContext->pcProgramCounter -= 2;

}


/***************************************************************************
Function    :  look_up
Description :  Look up a token's internal representation in the token table.
Returns     :
***************************************************************************/
int look_up(char *s)
{
    int i;
    char *p;
    int iToken = 0;   /*unknown token*/

    /*Convert to lower case*/
    p = s;
    while(*p)
    {
        *p = tolower(*p);
        p++;
    }

    /*Check if token is a command*/
    for(i=0; *asCommandTable[i].acName; i++)
    {
        if(!strcmp(asCommandTable[i].acName, s))
        {
            iToken = asCommandTable[i].eToken;
            psContext->eTokenType = COMMAND;
            break;
        }
    }

    if (!iToken)
    {
        /*Check if token is a function*/
        for(i=0; *asFunctionTable[i].acName; i++)
        {
            if(!strcmp(asFunctionTable[i].acName, s))
            {
                iToken = asFunctionTable[i].eToken;
                psContext->eTokenType = COMMAND;
                break;
            }
        }
    }

    return(iToken);

}


/***************************************************************************
Function    :  find_command
Description :  Find a token matching the string.
Returns     :  token enumeration value or 0 if no matching token is found
***************************************************************************/
int find_command(char *acString)
{
    int i;
    int iToken = 0;   /*unknown token*/


    /*Check if token is a command*/
    for(i=0; *asCommandTable[i].acName; i++)
    {
        if(!strcasecmp(asCommandTable[i].acName, acString))
        {
            iToken = asCommandTable[i].eToken;
            break;
        }
    }

    return(iToken);

}


/***************************************************************************
Function    :  find_function
Description :  Find a token matching the string.
Returns     :  token enumeration value or 0 if no matching token is found
***************************************************************************/
int find_function(char *acString)
{
    int i;
    int iToken = 0;   /*unknown token*/

    //printf("find function called %s...", acString);

    /*Check if token is a function*/
    for(i=0; *asFunctionTable[i].acName; i++)
    {
        //printf("%s vs %s\n", asFunctionTable[i].acName, acString);
        if(!strcasecmp(asFunctionTable[i].acName, acString))
        {
            iToken = asFunctionTable[i].eToken;
            //printf("Found function with token %d\n", iToken);
            break;            
        }
    }

    //if (iToken) printf("FOUND\n"); else printf("NOT FOUND\n");
    return(iToken);

}

/***************************************************************************
Function    :  find_logic
Description :  Find a token matching the string.
Returns     :  token enumeration value or 0 if no matching token is found
***************************************************************************/
int find_logic(char *acString)
{
    int i;
    int iToken = 0;   /*unknown token*/



    /*Check if token is a function*/
    for(i=0; *asLogicTable[i].acName; i++)
    {
        //printf("%s vs %s\n", asFunctionTable[i].acName, acString);
        if(!strcasecmp(asLogicTable[i].acName, acString))
        {
            iToken = asLogicTable[i].eToken;
            break;            
        }
    }

    return(iToken);

}


/***************************************************************************
Function    :  isdelim
Description :  Return true if c is a delimiter.
Returns     :
***************************************************************************/
int isdelim(char c)
{
    if(strchr(" ;,+-<>/*^=()", c) || c==9 || c=='\r' || c=='\n' || c==0)   // added \n for UNIX EOL
        return 1;

  return 0;
}

/***************************************************************************
Function    :  iswhite
Description :  Return 1 if c is space or tab.
Returns     :
***************************************************************************/
int iswhite(char c)
{
  if(c==' ' || c=='\t') return 1;
  else return 0;
}




/***************************************************************************
Function    :  get_StringVariable
Description :  Returns a pointer to the string variable's value
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
char *get_StringVariable(char *pcName)
{
    char *pacValue = NULL;
    int iIndex;

    iIndex = find_Variable(pcName, STRINGVARIABLE);

    if (iIndex >= 0)
    {
        /*Must update the date and time prior to a read*/
        if (strcmp(pcName, "time$") == 0) update_DateAndTime();
        if (strcmp(pcName, "date$") == 0) update_DateAndTime();
        if (strcmp(pcName, "day$") == 0) update_DateAndTime();

        /*Must clear the inkey after it has been used*/
        if (strcmp(pcName, "inkey$") == 0) bClearInkey = 1;

        pacValue = psContext->asStringVariables[iIndex].acValue;
    }
    else
    {
        printf("\nError: Attempt to use uninitialised variable %s\n", pcName);
        syntax_error(SYNTAX);
    }

    return(pacValue);
}

/***************************************************************************
Function    :  get_StringArrayVariable
Description :  Get the a pointer to a string variable
Returns     :  Value
***************************************************************************/
char *get_StringArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM])
{
    double fValue = 0;
    char *pcValue = NULL;

    pcValue = find_ArrayVariable(pcName, aiIndex);

    if (!pcValue)
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(pcValue);
}


/***************************************************************************
Function    :  get_FloatVariable
Description :  Get the value of a float variable
Returns     :  Value
***************************************************************************/
double get_FloatVariable(char *pcName)
{
    double fValue = 0;
    int iIndex;

    iIndex = find_Variable(pcName, FLOATVARIABLE);

    if (iIndex >= 0)
    {
        fValue = psContext->asFloatVariables[iIndex].fValue;
    }
    else
    {
        printf("\nError: Attempt to use uninitialised variable %s\n", pcName);
        syntax_error(NOT_VAR);
    }

    return(fValue);
}

/***************************************************************************
Function    :  get_FloatArrayVariable
Description :  Get the value of a float variable
Returns     :  Value
***************************************************************************/
double get_FloatArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM])
{
    double fValue = 0;
    double *pfValue = NULL;

    pfValue = (double *)find_ArrayVariable(pcName, aiIndex);

    if (pfValue)
    {
        fValue = *pfValue;
    }
    else
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(fValue);
}


/***************************************************************************
Function    :  get_IntegerVariable
Description :  Get the value of an integer variable
Returns     :  Value
***************************************************************************/
int get_IntegerVariable(char *pcName)
{
    int iValue = 0;
    int iIndex;

    iIndex = find_Variable(pcName, INTEGERVARIABLE);

    if (iIndex >= 0)
    {
        iValue = psContext->asIntegerVariables[iIndex].iValue;
    }
    else
    {
        printf("\nError: Attempt to use uninitialised variable %s\n", pcName);
        syntax_error(NOT_VAR);
    }

    return(iValue);
}


/***************************************************************************
Function    :  get_IntegerArrayVariable
Description :  Get the value of an integer variable
Returns     :  Value
***************************************************************************/
int get_IntegerArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM])
{
    int iValue = 0;
    int *piValue = NULL;

    piValue = (int *)find_ArrayVariable(pcName, aiIndex);

    if (piValue)
    {
        iValue = *piValue;
    }
    else
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(iValue);
}


/***************************************************************************
Function    :  set_StringVariable
Description :  Set the string variable's value.
               If the variable does not exist then it is created.
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
int set_StringVariable(char *pcName, char *pcValue)
{
    int iIndex = -1;
    int iStatus = 0;

    // find the variable
    iIndex = find_Variable(pcName, STRINGVARIABLE);
    if (iIndex == -1) iIndex = create_Variable(pcName, STRINGVARIABLE);

    // set the value
    strncpy(psContext->asStringVariables[iIndex].acValue, pcValue, STRING_VAR_LEN);
    psContext->asStringVariables[iIndex].acValue[STRING_VAR_LEN-1] = 0;

    return(iStatus);
}

/***************************************************************************
Function    :  set_StringArrayVariable
Description :  Set the value of an integer variable
Returns     :  Value
***************************************************************************/
int set_StringArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], char *pcValue)
{
    char *pcVariable = NULL;
    int iStatus = 0;

    pcVariable = find_ArrayVariable(pcName, aiIndex);

    if (pcVariable)
    {
        strncpy(pcVariable, pcValue, STRING_VAR_LEN);
        pcVariable[STRING_VAR_LEN-1] = 0;
    }
    else
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(iStatus);
}


/***************************************************************************
Function    :  set_FloatVariable
Description :  Set the value of a float variable
Returns     :  Value
***************************************************************************/
int set_FloatVariable(char *pcName, double fValue)
{
    int iIndex = -1;
    int iStatus = 0;

    // find the variable
    iIndex = find_Variable(pcName, FLOATVARIABLE);
    if (iIndex == -1) iIndex = create_Variable(pcName, FLOATVARIABLE);

    // set the value
    psContext->asFloatVariables[iIndex].fValue = fValue;


    return(iStatus);
}

/***************************************************************************
Function    :  set_FloatArrayVariable
Description :  Set the value of an integer variable
Returns     :  Value
***************************************************************************/
int set_FloatArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], double fValue)
{
    double *pfValue = NULL;
    int iStatus = 0;

    pfValue = (double *)find_ArrayVariable(pcName, aiIndex);

    if (pfValue)
    {
         *pfValue = fValue;
    }
    else
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(iStatus);
}


/***************************************************************************
Function    :  set_IntegerVariable
Description :  Set the value of an integer variable
Returns     :  Value
***************************************************************************/
int set_IntegerVariable(char *pcName, int iValue)
{
    int iIndex = -1;
    int iStatus = 0;

    // find the variable
    iIndex = find_Variable(pcName, INTEGERVARIABLE);
    if (iIndex == -1) iIndex = create_Variable(pcName, INTEGERVARIABLE);

    // set the value
    psContext->asIntegerVariables[iIndex].iValue = iValue;


    return(iStatus);
}

/***************************************************************************
Function    :  set_IntegerArrayVariable
Description :  Set the value of an integer variable
Returns     :  Value
***************************************************************************/
int set_IntegerArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], int iValue)
{
    int *piValue = NULL;
    int iStatus = 0;

    piValue = (int *)find_ArrayVariable(pcName, aiIndex);

    if (piValue)
    {
         *piValue = iValue;
    }
    else
    {
        printf("\nError: Invalid array access %s(%d, %d, %d, %d)\n", pcName, aiIndex[0], aiIndex[1], aiIndex[2], aiIndex[3]);
        syntax_error(NOT_VAR);
    }

    return(iStatus);
}




/***************************************************************************
Function    :  get_Bracket
Description :  Get a bracket
Returns     :
***************************************************************************/
void get_Bracket(char cBracket)
{
    get_token();
    if (*psContext->acToken != cBracket)
    {
	    if (cBracket == ')')
        {
            syntax_error(UNBAL_PARENS);
        }
        else
        {
            syntax_error(MISS_PARENS);
        }
    }
}
