/******************************************************************************
File:		basic_commands
Description:This file contains the standard BASIC commands.
******************************************************************************/

/*Include Files*/
#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "setjmp.h"
#include "math.h"
#include "ctype.h"
#include "string.h"
//#include "snmp_def.h"
#include "basic.h"
//#include "Gpib_cmd.h"
//#include "Relay.h"
#include "hc_task.h"
#include "shell.h"


/*External Variables*/
extern tsBasicContext *psContext;
extern char bScriptFileActive;                 //indicates a script is running
extern char bTraceActive;                             //trace Winndow is open

/*Public Variable*/



/*Private Variables*/
//CHAR_INFO ConsoleBuffer[80*3000];

/*Local Prototypes*/
void find_endif();


/***************************************************************************
Function    :  basic_Let
Description :  Assign a variable a value.
Returns     :  Nothing
***************************************************************************/
void basic_Let(void)
{
	int iValue;
    double fValue;
	char cDestinationType;
	char acValue[3000];
    char acName[3000];
    int iIndex;
    int iIndexArray[MAX_ARRAY_DIM];

	// get the variable name
	cDestinationType = get_token();
    //strcpy(acName, psContext->acToken);
    STRNCPY(acName, psContext->acToken, sizeof(acName));
    
    // check if this is an array variable
    if (psContext->pcProgramCounter[0] == '(')
    {
        // get the opening bracket
        get_token(); 

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

	    // get the equals sign
	    get_token();

	    if(*psContext->acToken != '=')
	    {
		    syntax_error(EQUALS_EXP);
		    return;
        }

        // set array value
        switch(cDestinationType)
        {
        case INTEGERVARIABLE:
		    eval_IntegerExpression(&iValue);
            set_IntegerArrayVariable(acName, iIndexArray, iValue);
            break;

        case FLOATVARIABLE:
		    eval_FloatExpression(&fValue);
		    set_FloatArrayVariable(acName, iIndexArray, fValue);
            break;

        case STRINGVARIABLE:
            eval_StringExpression(acValue, sizeof(acValue));
            set_StringArrayVariable(acName, iIndexArray, acValue);
            break;

        default:
            basic_printf("Left hand side of assignment must be a variable\n");
            syntax_error(SYNTAX);
            break;
        }
    }
    else
    {
         
        //putback();

	    // get the equals sign
	    get_token();

	    if(*psContext->acToken != '=')
	    {
		    syntax_error(EQUALS_EXP);
		    return;
	    }

        switch(cDestinationType)
        {
        case INTEGERVARIABLE:
		    eval_IntegerExpression(&iValue);
            set_IntegerVariable(acName, iValue);
            break;

        case FLOATVARIABLE:
		    eval_FloatExpression(&fValue);
		    set_FloatVariable(acName, fValue);
            break;

        case STRINGVARIABLE:
            eval_StringExpression(acValue, sizeof(acValue));
            set_StringVariable(acName, acValue);
            break;
    
        default:
            basic_printf("Left hand side of assignment must be a variable\n");
            syntax_error(SYNTAX);
            break;
        }
    }
}




/***************************************************************************
Function    :  basic_Print
Description :  Execute a simple version of the BASIC Print statement.
Returns     :  Nothing
***************************************************************************/
void basic_Print(void)
{
    //int err;
    //char acOutput[5000];  original PC version
    char acOutput[128];  // max size supported by ring buffer used for http shell

    eval_StringLine(acOutput, sizeof(acOutput), 1);

    basic_printf("%s", acOutput);
    //err = fprintf(0, "%s", acOutput);

    //printf("%s", acOutput);

    //basic_printf("MONKEY\n");

    //pico_send_async_text(acOutput);
    //shell_print_string(acOutput);  // this one blocks if the queue is full, stalling ther basic program but reducing data loss

}


/***************************************************************************
Function    :  basic_Goto
Description :  Execute a GOTO statement.
Returns     :  Nothing
***************************************************************************/
void basic_Goto(void)
{
  char *loc;

  get_token(); /* get label to go to */

  /*Automatically append a colon to text label names*/
  if (psContext->eTokenType == FLOATVARIABLE)
  {
    strcat(psContext->acToken, ":");
  }

  /* find the location of the label */
  loc = find_label(psContext->acToken);
  if(loc==NULL)
    syntax_error(UNDEF_LAB); /* label not defined */

  else psContext->pcProgramCounter = loc;  /* start program running at that loc */
}


/***************************************************************************
Function    :  basic_On
Description :  Execute an ON..GOTO or ON..GOSUB statement.
Returns     :  Nothing
***************************************************************************/
void basic_On(void)
{
    int iBranchNumber;
    char cBranchType;
    char *pcSubRoutine;

    /*Get branch number*/
    get_token();

    switch(psContext->eTokenType)
    {

    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &iBranchNumber);
        break;

    case NUMBER:
        sscanf(psContext->acToken, "%d", &iBranchNumber);
        break;

    default:
        putback();
        eval_IntegerExpression(&iBranchNumber);
        break;

    }

    /*Get GOTO or GOSUB*/
    get_token();
    cBranchType = psContext->eToken;
    if ((cBranchType != GOTO) && (cBranchType != GOSUB))
    {
        basic_printf("Expected GOTO or GOSUB\n");
        syntax_error(SYNTAX);
    }

    if ((iBranchNumber < 0) || (iBranchNumber > 100))
    {
        basic_printf("Error: ridiculous value in ON statement\n");
        syntax_error(SYNTAX);
    }

    iBranchNumber--;

    while(iBranchNumber > 0)
    {
        /*get label or line number*/
        get_token();
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

        /*get comma*/
        if (get_token() != DELIMITER) break; //comma
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

        iBranchNumber--;
    }


    if (iBranchNumber == 0)
    {
        /*Label found*/
        switch(cBranchType)
        {
            case GOTO:
                basic_Goto();
                break;

            case GOSUB:
                get_token();

                /*Automatically append a colon to text label names*/
                if (psContext->eTokenType == FLOATVARIABLE)
                {
                    strcat(psContext->acToken, ":");
                }

                /*find the label to call*/
                pcSubRoutine = find_label(psContext->acToken);
                if (pcSubRoutine==NULL)
                {
                    syntax_error(UNDEF_LAB); /* label not defined */
                }
                else
                {
                    find_eol();
                    gpush(psContext->pcProgramCounter);         /*save place to return to*/
                    psContext->pcProgramCounter = pcSubRoutine; /*start program running subroutine*/
                }

                break;
        }

    }
    else
    {
        find_eol();
    }

}





/***************************************************************************
Function    :  basic_If
Description :  Execute an IF statement.
Returns     :  Nothing
***************************************************************************/
void basic_If(void)
{
    int nNextToken;
    int iLeftSideTokenType;


    int nNumLeft;
    double fNumLeft;
    int nMatch;                        /* 0 if no match, 1 if condition true */

    void *pvProgPtrTemp;

    /*Take a peek at the next token*/
    nNextToken = get_token();
    putback();

    nMatch = 0;

    if ((nNextToken != STRINGVARIABLE) &&           //might need to add ( here
        (nNextToken != FLOATVARIABLE) &&
        (nNextToken != INTEGERVARIABLE) &&
        *psContext->acToken != '(')
    {
        syntax_error(NOT_VAR);
        return;
    }

    /*Remember what type of expression we are evalutating*/
    iLeftSideTokenType = nNextToken;

    if (nNextToken == STRINGVARIABLE)
    {
        //nMatch = eval_StringLogic();

        // test string comparison added to numeric expressions
        eval_NumericExpression(&nNumLeft, &fNumLeft);
        nMatch = nNumLeft;
    }
    else if ((nNextToken == FLOATVARIABLE) || (nNextToken == INTEGERVARIABLE) || (*psContext->acToken == '('))
    {
        /* Get variable */
        eval_NumericExpression(&nNumLeft, &fNumLeft);
        nMatch = nNumLeft;
    }
    else
    {
        /*Big! Mistake*/
        syntax_error(NOT_VAR);       
    }
    
    /* Process THEN token */
    get_token(); 

    // Newman added in 2026  -- allow THEN to be on newline
    while (psContext->eToken == EOL)
    {
        get_token();
    }   

    if ( psContext->eToken != THEN )
    {
        syntax_error( THEN_EXP );
        return;
    }

    /* search for ELSE on this line */
    pvProgPtrTemp = psContext->pcProgramCounter; /* save location in file */
    do
    {
        get_token();
    } while ( ( psContext->eToken != EOL ) && ( psContext->eToken != ELSE ) );

    if ( psContext->eToken == ELSE )
        psContext->iThenElseLine = 1;
    else
        psContext->iThenElseLine = 0;

    psContext->pcProgramCounter = pvProgPtrTemp;

    if ( nMatch != 0 )
    {
        /* condition is true, proceed executing tokens as discovered */
        return;
    }
    else
    {
        /* condition is false, skip to ELSE, end-of-line or ENDIF */
        if ( psContext->iThenElseLine == 1 )
        {
            do
            {
                get_token();
            } while ( psContext->eToken != ELSE );                  /* skip to end of line */

            psContext->iThenElseLine = 0;
            return;
        }

        /* determine if any tokens remain on line */
        get_token();
        if ( ( psContext->eToken == REM ) || ( psContext->eToken == EOL ) )
        {
            /* Search for next endif or else, whichever comes first */
            find_endiforelse();
        }
        else
        {
            /* Tokens on line indicates that we just skip to end of line */
            find_eol();
        }
    }
}

#ifdef OLD_AND_BUGGY
/***************************************************************************
Function    :  find_endiforelse
Description :  Finds the end of an IF block
Returns     :  Nothing
***************************************************************************/
void find_endiforelse()
{
    int num_begin = 0;
    int num_else = 0;

    do
    {
        // Get the next token
        get_token();

        if ( psContext->eToken == REM )
        {
            basic_Rem();
            //if ( num_else > 0 )    removed 28 August 2004 becuase I have not idea what it is for
            //    num_else--;
            continue;              // added 28 August 2004
        }

        if ( psContext->eToken == EOL )
        {
            //if ( num_else > 0 )    removed 28 August 2004 becuase I have not idea what it is for
            //    num_else--;
            continue;              // added 28 August 2004
        }

        // if it's a then with no remaining tokens, then increment the count
        if (psContext->eToken == THEN)
        {
            get_token();
            if ( ( psContext->eToken == REM ) || ( psContext->eToken == EOL ) )
                num_begin++;
            else
                num_else++;

            putback();
            continue;              // added 28 August 2004. Since whave done a putback we MUST get_token() again!
        }

        // if it's an end, with a following if, then decrement the count
        if (psContext->eToken == END)
        {
            get_token();
            if ( psContext->eToken == IF )
                num_begin--;
			else
				putback();  // in case we have a conditional END !

            continue;              // added 28 August 2004. Since whave done a putback we MUST get_token() again!
        }

        // if else encountered, then stop if count is zero
        if (psContext->eToken == ELSE)
        {
            if ( num_else > 0 )
                num_else--;
            else if ( num_else < 0 )
                basic_error( TOO_MNY_ELSE );
            else if ( num_begin == 0 )
                break;
        }

        // if elseif encountered, then stop if count is zero
        if (psContext->eToken == ELSEIF)
        {
            if ( num_else > 0 )
            {
                num_else--;
                //num_begin--;                      //<------------new stuff 2 July 2004, assume elseif always follows block                
            }  
            else if ( num_else < 0 )
                basic_error( TOO_MNY_ELSE );
            else if ( num_begin == 1 )             // added 28 August 2004. As we did not decrement num_begin is 1 when we find elseif we want to execute
            {
                /* execute if statement */
                basic_If();
                break;
            }
        }

        // if end-of-file then error
        if (psContext->eToken == FINISHED)
        {
            basic_error( NO_ENDIF );
        }
    } while (num_begin > -1);
}
#endif


/***************************************************************************
Function    :  find_endiforelse
Description :  Finds the end of an IF block
Returns     :  Nothing
***************************************************************************/
void find_endiforelse()
{
    int found = 0;

    do
    {
        // Get the next token
        get_token();

        switch(psContext->eToken)
        {
        case REM:
            basic_Rem();
            break;

        case THEN:
            get_token();
            putback();

            if ((psContext->eToken == REM) || (psContext->eToken == EOL))
            {                
                find_eol();
                find_endif(); // skip remaining ELSEIF and ELSE blocks
            }
            else
            {
                // skip past any nested one line IF THEN ELSE statements -- we only want to see block statements
                find_eol();
            }
            break;

        case END:
            get_token();

            if (psContext->eToken == IF)
            {
                found = 1;
            }
            else
            {			
			    putback();
            }
            break;

        case ELSE:
            found = 1;  
            break;            

        case ELSEIF:
             found = 1;
             basic_If();             
             break;

        case FINISHED:
            syntax_error(NO_ENDIF);
            break;
        }

    } while (!found);
}


/***************************************************************************
Function    :  find_endif
Description :  Finds the logical end of an IF block.
               This must be an END IF.
               We must skip any nested IF/ELSE/ELSEIF blocks.
Returns     :  Nothing
***************************************************************************/
void find_endif()
{
    int num_begin = 0;

    do
    {
        // Get the basic_Next token
        get_token();

        // if it's a THEN with no remaining tokens, then increment the count
        if (psContext->eToken == THEN)
        {
            get_token();
            if ( ( psContext->eToken == REM ) || ( psContext->eToken == EOL ) )
                num_begin++;

            putback();
        }
        
		// if it's an ELSEIF then there must be a THEN on the same line
		// These two cancel one another so no change to the count
        if (psContext->eToken == ELSEIF)
        {
            /* determine if line begins a new block */
            while ( (psContext->eToken != THEN) &&
				    (psContext->eToken != REM) &&
					(psContext->eToken != EOL) )
                get_token();

            if (psContext->eToken != THEN)
			{
				basic_printf("ERROR: ELSEIF without THEN\n");
                syntax_error( THEN_EXP );            
			}
        }

        // if it's an end, with a following if, then decrement the count
        if (psContext->eToken == END)
        {
            get_token();
            if ( psContext->eToken == IF )
                num_begin--;
			else
				putback();  // in case we have a conditional END !
        }

        // if end-of-file then error
        if (psContext->eToken == FINISHED)
        {
            syntax_error( NO_ENDIF );
        }
    } while (num_begin > -1);
}

/***************************************************************************
Function    :  basic_Else
Description :  Executes ELSE block
Returns     :  Nothing
***************************************************************************/
void basic_Else(void)
{
    if ( psContext->eToken == ELSEIF )
    {
        /* determine if line begins a new block */
        while ( (psContext->eToken != THEN) && (psContext->eToken != REM) && (psContext->eToken != EOL) )
            get_token();

        if (psContext->eToken != THEN)
            syntax_error( THEN_EXP );

        /* check the basic_Next token to determine if a new block */
        get_token();
        if ( ( psContext->eToken == REM ) || ( psContext->eToken == EOL ) )
        {
            putback();
            find_endif();
        }
        else
        {
            putback();
            find_eol();
        }
    }
    else
    {
        /* determine if more exists on this line */
        get_token();
        if ( ( psContext->eToken == REM ) || ( psContext->eToken == EOL ) )
        {
            putback();            
			find_endif();
        }
        else
        {
            putback();
            find_eol();
        }
    }
}



/***************************************************************************
Function    :  basic_For
Description :  Execute a FOR loop.
Returns     :  Nothing
***************************************************************************/
void basic_For(void)
{
    tsForStack i;
    double fValue;
    double fStep = 1;

    /*Read the control variable*/
    get_token();
    if((psContext->eTokenType != INTEGERVARIABLE) && (psContext->eTokenType != FLOATVARIABLE))
    {
        syntax_error(NOT_VAR);
        return;
    }

    /*Save control variable's type and index*/
    i.eCounterType = psContext->eTokenType; 
    i.iVariableIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (i.iVariableIndex == -1) i.iVariableIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    /* read the equals sign */
    get_token();
    if(*psContext->acToken!='=')
    {
        syntax_error(EQUALS_EXP);
        return;
    }

    /*Get initial value*/
    eval_FloatExpression(&fValue);

    if (i.eCounterType == INTEGERVARIABLE)
    {
        psContext->asIntegerVariables[i.iVariableIndex].iValue = (int)fValue;
    }
    else
    {
        psContext->asFloatVariables[i.iVariableIndex].fValue = fValue;
    }

    /*read and discard the TO*/
    get_token();
    if(psContext->eToken!=TO) syntax_error(TO_EXP);

    /*Get Target value*/
    eval_FloatExpression(&i.fTarget);

    /*Check for optional STEP keyword*/
    get_token();
    if(psContext->eToken == STEP)
    {
        /*Get step size*/
        eval_FloatExpression(&fStep);
    }
    else
    {
        putback();
        fStep = 1;
    }

    /*if loop can execute at least once, push info on stack.
      Loop may execute once if start and finish values are equal!*/
    if(((fValue <= i.fTarget) && (fStep > 0)) ||
       ((fValue >= i.fTarget) && (fStep < 0)))
    {
        i.loc = psContext->pcProgramCounter;
        i.fStepSize = fStep;
        fpush(i);
    }
    else
    {
        /* otherwise, skip loop code altogether */
        while(psContext->eToken!=NEXT) get_token();
    }
}


/***************************************************************************
Function    :  basic_Next
Description :  Execute a NEXT statement.
Returns     :  Nothing
***************************************************************************/
void basic_Next(void)
{
    tsForStack i;
    int bLoopComplete = 0;
    double fValue;

    i = fpop(); /* read the loop info */

    /*Increment control variable*/
    if (i.eCounterType == INTEGERVARIABLE)
    {
        psContext->asIntegerVariables[i.iVariableIndex].iValue += (int)i.fStepSize;
        fValue = (double)(psContext->asIntegerVariables[i.iVariableIndex].iValue);
    }
    else
    {
        psContext->asFloatVariables[i.iVariableIndex].fValue += i.fStepSize;
        fValue = psContext->asFloatVariables[i.iVariableIndex].fValue;
    }
     
  
    /*Check for loop termination*/
    if((fValue > i.fTarget) && (i.fStepSize > 0)) bLoopComplete = 1;
  
    if((fValue < i.fTarget) && (i.fStepSize < 0)) bLoopComplete = 1;

    if(!bLoopComplete)
    {
        /*perform another iteration*/
        fpush(i);  
        psContext->pcProgramCounter = i.loc;
    }
}


/***************************************************************************
Function    :  fpush
Description :  Push the FOR stack.
Returns     :  Nothing
***************************************************************************/
void fpush(tsForStack i)
{
   if(psContext->iTopOfForStack>=FOR_NEST)
    syntax_error(TOO_MNY_FOR);

  psContext->sForStack[psContext->iTopOfForStack] = i;
  psContext->iTopOfForStack++;
}


/***************************************************************************
Function    :  fpop
Description :  Pop the FOR stack.
Returns     :  Nothing
***************************************************************************/
tsForStack fpop(void)
{
  psContext->iTopOfForStack--;
  if(psContext->iTopOfForStack<0) syntax_error(NEXT_WO_FOR);
  return(psContext->sForStack[psContext->iTopOfForStack]);
}

/***************************************************************************
Function    :  basic_While
Description :  Execute a WHILE loop.
Returns     :  Nothing
***************************************************************************/
void basic_While(void)
{
    tsWhileStack i;
    int  iTruth = 0;
    int iWendCount;
    int iWhileCount;
    int iNextToken;

    // move back to start of WHILE command
    putback();
    get_token();
    if (psContext->eToken == WHILE) putback(); else basic_printf("Expected while but got %s\n", psContext->acToken);

    i.loc = psContext->pcProgramCounter;

    get_token();  // while
    get_Bracket('(');

    // take a peek at the next token
    iNextToken = get_token();
    putback();

    if (iNextToken == STRINGVARIABLE)
    {
        //iTruth = eval_StringLogic();

        // test string comparison added to numeric expressions
        eval_NumericExpression(&iTruth, NULL);
    }
    else if ((iNextToken == FLOATVARIABLE)   ||
             (iNextToken == INTEGERVARIABLE) ||
             (*psContext->acToken == '('))
    {
        eval_NumericExpression(&iTruth, NULL);
    }
    else
    {
        syntax_error(NOT_VAR);       
    }

    get_Bracket(')');

    if(iTruth)
    {        
        wpush(i);
    }
    else
    {
        /* skip loop code */
        iWhileCount = 1;
        iWendCount = 0;
        while(iWendCount != iWhileCount)
        {
            get_token();
            if (psContext->eToken == WHILE)
            {
                iWhileCount++;
            }
            else if (psContext->eToken == WEND)
            {
                iWendCount++;
            }
            else if (psContext->eToken == FINISHED)
            {
                basic_printf("Couldn't find closing WEND\n");
                psContext->pcProgramCounter = i.loc;
                syntax_error(SYNTAX);
            }
        }
    }
}


/***************************************************************************
Function    :  basic_Wend
Description :  Execute a WEND statement.
Returns     :  Nothing
***************************************************************************/
void basic_Wend(void)
{
    tsWhileStack i;

    i = wpop(); 
        
    psContext->pcProgramCounter = i.loc;

    //wpush(i);         
}


/***************************************************************************
Function    :  wpush
Description :  Push the WHILE stack.
Returns     :  Nothing
***************************************************************************/
void wpush(tsWhileStack i)
{
   if(psContext->iTopOfWhileStack>=WHILE_NEST)
    syntax_error(TOO_MNY_WHILE);

  psContext->sWhileStack[psContext->iTopOfWhileStack] = i;
  psContext->iTopOfWhileStack++;
}


/***************************************************************************
Function    :  wpop
Description :  Pop the WHILE stack.
Returns     :  Nothing
***************************************************************************/
tsWhileStack wpop(void)
{
  psContext->iTopOfWhileStack--; 
  if(psContext->iTopOfWhileStack<0) syntax_error(WEND_WO_WHILE);
  return(psContext->sWhileStack[psContext->iTopOfWhileStack]);
}


/***************************************************************************
Function    :  basic_Input
Description :  Execute a simple form of the BASIC INPUT command.
Returns     :  Nothing
***************************************************************************/
void basic_Input(void)
{
	// int i = 0;
    // float f = 0;
	// char *pcInputText;
    // char acPromptText[200] = {0};
    // int iIndex;
    // char acWindowTitleGet[128];        /* array to retrieve window title */
    // HWND hWnd = NULL;                           /* handle of this window */


    // /*If trace window is open then move SNMP window to the front for INPUT*/
    // if (bTraceActive)
    // {

    //     if ((GetConsoleTitle(acWindowTitleGet, sizeof(acWindowTitleGet)) > 0))
    //     {
    //         /*Convoluted way of finding window hanlde*/
    //         hWnd = FindWindow("ConsoleWindowClass", acWindowTitleGet);
    //     }

    //     if (hWnd) BringWindowToTop(hWnd);
    // }

    // get_token(); /* see if prompt string is present */

	// if(psContext->eTokenType==QUOTE)
	// {
	// 	sprintf(acPromptText, "%s",psContext->acToken); /* if so, print it and check for comma */
	// 	get_token();

	// 	if(*psContext->acToken!=',') basic_error(SYNTAX);
	// 		get_token();
	// }
	// else
	// {
	// 	basic_printf("? "); /* otherwise, prompt with ? */
	// }

	// /*Get the user basic_Input*/
	// pcInputText = getCommand(acPromptText, 1);

    // /*Erase WAIT message and Display Input text*/
    // basic_printf("\r                                                                               ");
    // basic_printf("\r%s    ", acPromptText);
	// putch(BACKSPACE); putch(BACKSPACE); putch(BACKSPACE); putch(BACKSPACE);
    // basic_printf("%s\n", pcInputText);

	// /*Get the basic_Input variable name*/
    // iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    // if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

	// if(psContext->eTokenType == STRINGVARIABLE)
	// {
    //     /* read basic_Input */
    //     sscanf(pcInputText, "%s", psContext->asStringVariables[iIndex].acValue);
	// }
	// else if(psContext->eTokenType == FLOATVARIABLE)
	// {
	// 	/* read basic_Input */
    //     sscanf(pcInputText, "%f", &f);
	// 	psContext->asFloatVariables[iIndex].fValue = (double)f; /* store it */
	// }
	// else
	// {
	// 	/* read basic_Input */
    //     sscanf(pcInputText, "%d", &i);
	// 	psContext->asIntegerVariables[iIndex].iValue = i; /* store it */
	// }

} /*end Input*/


/***************************************************************************
Function    :  basic_Gosub
Description :  Execute a GOSUB command.
Returns     :  Nothing
***************************************************************************/
void basic_Gosub(void)
{
  char *loc;

  get_token();

  /*Automatically append a colon to text label names*/
  if (psContext->eTokenType == FLOATVARIABLE)
  {
    strcat(psContext->acToken, ":");
  }

  /* find the label to call */
  loc = find_label(psContext->acToken);
  if(loc==NULL)
    syntax_error(UNDEF_LAB); /* label not defined */
  else {
    gpush(psContext->pcProgramCounter); /* save place to return to */
    psContext->pcProgramCounter = loc;  /* start program running at that loc */
  }
}


/***************************************************************************
Function    :  basic_Function
Description :  Execute a user defined function.
Returns     :  Nothing
***************************************************************************/
void basic_Function(void)
{
    char *loc;
    int iParamNum;
    int iFuncNum;


    // find the user function to call
    loc = find_UserFunction(psContext->acToken);

    // find the function number
    for(iFuncNum=0; iFuncNum<NUM_FUNC; iFuncNum++)
    {       
        if(!strcmp(psContext->sUserFunctionTable[iFuncNum].name,psContext->acToken))
        {
            break;
        }
    }


    if(loc==NULL)
    {
        syntax_error(UNDEF_FUNC); /* label not defined */
    }
    else
    {
        // read call parameters
        get_Bracket('(');
        for (iParamNum=0; iParamNum < 10; iParamNum++)
        {
            get_token();
            if (psContext->acToken[0] == ',') get_token();
            if (psContext->acToken[0] == ')')
            {
                break;
            }

            putback();

            switch(psContext->sUserFunctionTable[iFuncNum].asParameters[iParamNum].eParamType)
            {
            case INTEGERVARIABLE:
                eval_IntegerExpression(&psContext->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamInt.iValue);
                break;

            case FLOATVARIABLE:
                eval_FloatExpression(&psContext->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamFlt.fValue);
                break;

            case STRINGVARIABLE:
                eval_StringExpression(psContext->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamStr.acValue, sizeof(psContext->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamStr.acValue));
                break;

            default:
                basic_printf("Too many parameters in function call\n");
                syntax_error(SYNTAX);
                break;
            }
        }
       
        basic_InterpretFunction(loc, iFuncNum);

    }
}

/***************************************************************************
Function    :  basic_Return
Description :  Return from GOSUB or user defined function.
Returns     :  Nothing
***************************************************************************/
void basic_Return(void)
{
    int iIntIndex;
    int iFltIndex;



    get_token();

    if (psContext->acToken[0] == '(')
    {
        // evaluate return value
        iIntIndex = find_Variable("returnvalue%", INTEGERVARIABLE);
        iFltIndex = find_Variable("returnvalue",  FLOATVARIABLE);

        get_token();

        if (psContext->acToken[0] != ')')
        {
            switch(psContext->eTokenType)
            {

            case QUOTE:
            case STRINGVARIABLE:
	    	    putback();
                eval_StringExpression(get_StringVariable("returnvalue$"), STRING_VAR_LEN);
 
                sscanf(get_StringVariable("returnvalue$"), "%d", &psContext->asIntegerVariables[iIntIndex].iValue);
                sscanf(get_StringVariable("returnvalue$"), "%f", &psContext->asFloatVariables[iFltIndex].fValue);
                break;

            default:
                putback();
                eval_NumericExpression(&psContext->asIntegerVariables[iIntIndex].iValue, &psContext->asFloatVariables[iFltIndex].fValue);

                /*avoid printing decimal point if whole number*/
                if ((psContext->asFloatVariables[iFltIndex].fValue - (double)psContext->asIntegerVariables[iIntIndex].iValue) == 0)
                {
                    sprintf(get_StringVariable("returnvalue$"),"%d", psContext->asIntegerVariables[iIntIndex].iValue);
                }
                else
                {
                    sprintf(get_StringVariable("returnvalue$"),"%g", psContext->asFloatVariables[iFltIndex].fValue);
                }
                break;
            }
        }

        // terminate this instance of the interpreter and return to function callers context
        bScriptFileActive = 0;

    }
    else
    {
        putback();
        psContext->pcProgramCounter = gpop();
    }



}

/***************************************************************************
Function    :  gpush
Description :  Push GOSUB stack.
Returns     :  Nothing
***************************************************************************/
void gpush(char *s)
{
  psContext->iTopOfGosubStack++;

  if(psContext->iTopOfGosubStack==SUB_NEST) {
    syntax_error(TOO_MNY_GOSUB);
    return;
  }

  psContext->cGosubStack[psContext->iTopOfGosubStack]=s;
}

/***************************************************************************
Function    :  gpop
Description :  Pop GOSUB stack.
Returns     :  Nothing
***************************************************************************/
char *gpop(void)
{
  if(psContext->iTopOfGosubStack==0) {
    syntax_error(RET_WO_GOSUB);
    return 0;
  }

  return(psContext->cGosubStack[psContext->iTopOfGosubStack--]);
}




/***************************************************************************
Function    :  basic_Rem
Description :  Remark
Returns     :  Nothing
***************************************************************************/
void basic_Rem(void)
{
	/*Ignore the remainder of the line since it contains a comment*/
	find_eol();
}




/***************************************************************************
Function    :  basic_System
Description :  Pass a command to the system shell
Returns     :  Nothing
***************************************************************************/
void basic_System(void)
{
	char acTemp[1500];
    int iReturnValue;
    int fReturnValue;
    char acReturnValue[1500];

    get_Bracket('(');

    eval_StringExpression(acTemp, sizeof(acTemp));

    get_Bracket(')');

	//_flushall();	
    iReturnValue = system(acTemp);
    
    fReturnValue = iReturnValue;
    sprintf(acReturnValue, "%d", iReturnValue);

    set_IntegerVariable("returnvalue%", iReturnValue);
    set_FloatVariable("returnvalue", fReturnValue);
    set_StringVariable("returnvalue$", acReturnValue);

}


/***************************************************************************
Function    :  basic_Home
Description :  Moves current print position back to the start of the current
               line.
Returns     :  Nothing
***************************************************************************/
void basic_Home(void)
{
	basic_printf("\r");
}


/***************************************************************************
Function    :  basic_Chain
Description :  Load another script into this context optional appending
               the script to the current one.
Returns     :  Nothing
***************************************************************************/
void basic_Chain(void)
{
    // char *p;

	// p = strrchr(psContext->pcProgram, 'E');

    // *(p+3) = '\r';
    // *(p+4) = '\n';

    // get_token();
	
    // if(load_program(p+5, scriptPathName(psContext->acToken)))
    // {
    //     /*Reload Trace Window*/
    //     if (bTraceActive)
    //     {
    //         sock_TraceOpen(psContext->acFileName);
    //     }
    //     scan_labels();
    //     scan_UserFunctions();
    // }
    // else
    // {
	// 	basic_printf("Could not open BASIC script file %s\n", psContext->acToken);
    // }    
}


/***************************************************************************
Function    :  basic_Mid
Description :  MID$ A$ B$ x y where A$ is source, B$ is destination, x and y
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Mid(void)
{
    char *source = NULL;
    char *dest = NULL;
    int x = 0;
    int y = 0;
    int iIndex = 0;

    /* A$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    source = get_StringVariable(psContext->acToken);

    /* B$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    dest = psContext->asStringVariables[iIndex].acValue;

    /* get x */
    get_token();    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &x);
        break;

    case NUMBER:
        sscanf(psContext->acToken, "%d", &x);
        break;

    default:
        putback();
        eval_IntegerExpression(&x);
        break;
    }    
    
    /* get y */
    get_token();    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &y);
        break;

    case NUMBER:
        sscanf(psContext->acToken, "%d", &y);
        break;

    default:
        putback();
        eval_IntegerExpression(&y);
        break;
    } 

    //basic_printf("MID called on %s with %d and %d\n", source, x, y);
    
    if ((x > y) || (y > (int)strlen(source)))
    {
        strcpy(dest, "");
    }
    else
    {
        strncpy(dest, source+x, y-x+1);
        *(dest+y-x+1) = 0;
    }

    //basic_printf("Result:\n source = %s\n dest = %s\n", source, dest);
}


/***************************************************************************
Function    :  basic_Left
Description :  LEFT$ A$ B$ x where A$ is source, B$ is destination and x
               is the number of characters to copy.  This function copies
			   upto x characters starting from the far left end of A$ to B$
Returns     :  Nothing
***************************************************************************/
void basic_Left(void)
{
    char *source = NULL;
    char *dest = NULL;
    unsigned int x = 0;
    int iIndex = 0;

    /* A$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    source = get_StringVariable(psContext->acToken);

    /* B$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    dest = psContext->asStringVariables[iIndex].acValue;

    /* get x */
    get_token();    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &x);
        break;

    case NUMBER:
        sscanf(psContext->acToken, "%d", &x);
        break;

    default:
        putback();
        eval_IntegerExpression(&x);
        break;
    }    
    

    //basic_printf("LEFT$ called on %s with %d and %d\n", source, x, y);
    
    if (x > strlen(source))
    {
        x = strlen(source);
    }

	strncpy(dest, source, x);
    *(dest+x) = 0;
    

    //basic_printf("Result:\n source = %s\n dest = %s\n", source, dest);
}


/***************************************************************************
Function    :  basic_Right
Description :  RIGHT$ A$ B$ x where A$ is source, B$ is destination and x
               is the number of characters to copy.  This function copies
			   upto x characters starting from the far left end of A$ to B$
Returns     :  Nothing
***************************************************************************/
void basic_Right(void)
{
    char *source = NULL;
    char *dest = NULL;
    unsigned int x = 0;
    int iIndex = 0;

    /* A$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    source = get_StringVariable(psContext->acToken);

    /* B$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    dest = psContext->asStringVariables[iIndex].acValue;

    /* get x */
    get_token();    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &x);
        break;

    case NUMBER:
        sscanf(psContext->acToken, "%d", &x);
        break;

    default:
        putback();
        eval_IntegerExpression(&x);
        break;
    }    
    

    //basic_printf("RIGHT$ called on %s with %d and %d\n", source, x, y);
    
    if (x > strlen(source))
    {
        x = strlen(source);
    }

	strncpy(dest, source+strlen(source)-x, x);
    *(dest+x) = 0;
    

    //basic_printf("Result:\n source = %s\n dest = %s\n", source, dest);
}


/***************************************************************************
Function    :  basic_Ucase
Description :  UCASE$ A$ B$ where A$ is source, B$ is destination.
               This function copies A$ to B$ converting all lower case
			   letters to upper case.
Returns     :  Nothing
***************************************************************************/
void basic_Ucase(void)
{
    char *source = NULL;
    char *dest = NULL;
    unsigned int x = 0;
    int iIndex = 0;

    /* A$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    source = get_StringVariable(psContext->acToken);

    /* B$ */
    get_token();    
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    dest = psContext->asStringVariables[iIndex].acValue;

    //basic_printf("UCASE$ called on %s with %d and %d\n", source, x, y);
    
    for(x=0; x < strlen(source); x++)
    {
        dest[x] = toupper(source[x]);
    }

    dest[x] = 0;
    

    //basic_printf("Result:\n source = %s\n dest = %s\n", source, dest);
}


/***************************************************************************
Function    :  basic_AtoI
Description :  Converts a decimal string into an integer
Returns     :  Nothing
***************************************************************************/
void basic_AtoI(void)
{
    char * string;
    int iIndex;
    int result = 0;

    get_token();    /* A$ */
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    string = get_StringVariable(psContext->acToken);

    get_token();    /* x */
    if((psContext->eTokenType != INTEGERVARIABLE) && (psContext->eTokenType != FLOATVARIABLE))
	{
        syntax_error(SYNTAX);
	}

    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    result = atoi(string);

    if (psContext->eTokenType == INTEGERVARIABLE)
    {
        psContext->asIntegerVariables[iIndex].iValue = result;
    }
    else
    {
        psContext->asFloatVariables[iIndex].fValue = (double)result;
    }
}


/***************************************************************************
Function    :  basic_AtoIHex
Description :  Converts a hex string into an integer
Returns     :  Nothing
***************************************************************************/
void basic_AtoIHex(void)
{
    char * string;
    char temp[100] = {'0', 'x'};
    int result = 0;
    int iIndex;

    get_token();    /* A$ */
    if(psContext->eTokenType !=STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}

    string = get_StringVariable(psContext->acToken);

    get_token();    /* x */
    if((psContext->eTokenType != INTEGERVARIABLE) && (psContext->eTokenType != FLOATVARIABLE))
	{
        syntax_error(SYNTAX);
	}

    strcat(temp, string);

    sscanf(temp, "%x", &result);
    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    if (psContext->eTokenType == INTEGERVARIABLE)
    {
        psContext->asIntegerVariables[iIndex].iValue = result;
    }
    else
    {
        psContext->asFloatVariables[iIndex].fValue = (double)result;
    }
}


/***************************************************************************
Function    :  basic_ItoAHex
Description :  Converts an integer to a hex string
               ITOAHEX X A$
               X = integer to convert to string
               A$ = result
Returns     :  Nothing
***************************************************************************/
void basic_ItoAHex(void)
{
    int integer;
    char * string;
    char temp[100] = {'0', 'x'};
    int result = 0;
    int iIndex;

    /*Get the integer variable x*/
    eval_IntegerExpression(&integer);
    
    /*Find or create the string variable A$*/
    get_token(); 
    if(psContext->eTokenType != STRINGVARIABLE)
	{
        syntax_error(SYNTAX);
	}
    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);
    string = get_StringVariable(psContext->acToken);

    sprintf(string, "%x", integer);

}

/***************************************************************************
Function    :  basic_Open
Description :  Opens a file for I/O
Returns     :  Nothing
***************************************************************************/
void basic_Open(void)
{
	int var = 0;
    char acFileName[100];
    char *pcMode = "rt";
    int iFileNumber;
    int iIndex;

	/*Get Filename*/
    get_token();


	if(psContext->eTokenType==STRINGVARIABLE)
	{
		/*Get file name from string variable*/
        iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
        //strcpy(acFileName, psContext->asStringVariables[iIndex].acValue);
        STRNCPY(acFileName, psContext->asStringVariables[iIndex].acValue, sizeof(acFileName));
	}
	else
	{
		/*literal name*/
		//strcpy(acFileName, psContext->acToken);
        STRNCPY(acFileName, psContext->acToken, sizeof(acFileName));
	}

    /*Get Mode*/
     get_token();

    if (strcmp(psContext->acToken, "for") == 0)
    {
        /*Get mode specifier*/
        get_token();
        if (strcmp(psContext->acToken, "output") == 0)
        {
            /*Output mode*/
            pcMode = "wt";
        }
        else if (strcmp(psContext->acToken, "input") == 0)
        {
            /*Input mode*/
            pcMode = "rt";
        }
        else if (strcmp(psContext->acToken, "append") == 0)
        {
            /*Append mode*/
            pcMode = "at+";
        }
        else
        {
            syntax_error(SYNTAX);
        }

        /*Get AS*/
        get_token();
    }

    /*Should be at the AS token now*/
    if (strcmp(psContext->acToken, "as") != 0)
    {
        syntax_error(SYNTAX);
    }

    /*Get File Number*/
    get_token();

	if ((psContext->eTokenType == INTEGERVARIABLE) || (psContext->eTokenType == FLOATVARIABLE))
    {
        putback();
        eval_IntegerExpression(&iFileNumber);
    }
    else
    {
        iFileNumber = *psContext->acToken - '0';  //FIX!  won't work for 10
    }

    if ((iFileNumber < 0) || (iFileNumber > 10))
    {
        basic_printf("Error in OPEN: File Number out of range\n");
        syntax_error(SYNTAX);
    }

    /*Check if that file number is already in use*/
    if (psContext->apFileHandles[iFileNumber] == NULL)
    {
        /*Open the file*/
        psContext->apFileHandles[iFileNumber] = fopen(acFileName, pcMode);

        if (psContext->apFileHandles[iFileNumber] == NULL)
        {
            basic_printf("Error in OPEN: Could not open file\n");
            syntax_error(SYNTAX);
        }
    }
    else
    {
        basic_printf("Error in OPEN: File Number already in use\n");
        syntax_error(SYNTAX);
    }

} /*end basic_Open*/


/***************************************************************************
Function    :  basic_Close
Description :  Opens a file for I/O
Returns     :  Nothing
***************************************************************************/
void basic_Close(void)
{
	int var = 0;
    char *pcMode = "rt";
    int iFileNumber;


    /*Get File Number*/
    get_token();

	if ((psContext->eTokenType == INTEGERVARIABLE) || (psContext->eTokenType == FLOATVARIABLE))
    {
        putback();
        eval_IntegerExpression(&iFileNumber);
    }
    else
    {
        iFileNumber = *psContext->acToken - '0';
    }

    if ((iFileNumber < 0) || (iFileNumber > 10))
    {
        basic_printf("Error in CLOSE: File Number out of range (%d)\n", iFileNumber);
        syntax_error(SYNTAX);
    }

    /*Check if that file number is already in use*/
    if (psContext->apFileHandles[iFileNumber] != NULL)
    {
        fclose(psContext->apFileHandles[iFileNumber]);
        psContext->apFileHandles[iFileNumber] = NULL;
    }
    else
    {
        basic_printf("Error in CLOSE: File was not open\n");
        syntax_error(SYNTAX);
    }

} /*end basic_Close*/




/***************************************************************************
Function    :  basic_PrintToFile
Description :  Execute a simple version of the BASIC PRINT# statement.               
Returns     :  Nothing
***************************************************************************/
void basic_PrintToFile(void)
{
    char acOutput[5000];
    int iFileNumber;
    int err = 0;



    // get file number
    eval_IntegerExpression(&iFileNumber);

    if ((iFileNumber < 0) || (iFileNumber > 10))
    {
        basic_printf("Error in basic_Print#: File Number out of range\n");
        syntax_error(SYNTAX);
    }

    // check if that file number has been opened
    if (psContext->apFileHandles[iFileNumber] == NULL)
    {
        basic_printf("Error in basic_Print#: File not open\n");
        syntax_error(SYNTAX);
    }

    /*Get the comma*/
    get_token();
	if (psContext->eToken==EOL) putback();  //This allows for printing a blank line
 

    eval_StringLine(acOutput, sizeof(acOutput), 1);

    err = fprintf(psContext->apFileHandles[iFileNumber], "%s", acOutput);
}


#ifdef OLD_PRINT
/***************************************************************************
Function    :  basic_PrintToFile
Description :  Execute a simple version of the BASIC PRINT# statement.
               This function does not require the use of a large buffer!
               OK, so I'm lazy.  Most of the code here is a duplicate of the
               ordinary basic_Print function.
Returns     :  Nothing
NOTE        :  This function is now out of date.  It needs to be realigned
               with the normal print function (use get_variable functions).
***************************************************************************/
void basic_PrintToFile(void)
{
	int iAnswer;
	double fAnswer;
	int len=0, spaces;
	char last_delim;
	int var = 0;
    char *pcMode = "rt";
    int iFileNumber;
    char *pcTemp;

	/*Update date and time in case we are printing them out*/
	update_DateAndTime();

    /*Get File Number*/
    get_token();

	if ((psContext->eTokenType == INTEGERVARIABLE) || (psContext->eTokenType == FLOATVARIABLE))
    {
        putback();
        eval_IntegerExpression(&iFileNumber);
    }
    else
    {
        iFileNumber = *psContext->acToken - '0';
    }

    if ((iFileNumber < 0) || (iFileNumber > 10))
    {
        basic_printf("Error in basic_Print#: File Number out of range\n");
        basic_error(SYNTAX);
    }

    /*Check if that file number is already in use*/
    if (psContext->apFileHandles[iFileNumber] == NULL)
    {
        basic_printf("Error in basic_Print#: File not open\n");
        basic_error(SYNTAX);
    }

    /*Get the comma*/
    get_token();
	if (psContext->eToken==EOL) putback();  //This allows for printing a blank line

    do
	{
		last_delim = *psContext->acToken;
		get_token(); /* Get Next list item */

		if(psContext->eToken==EOL || psContext->eToken==FINISHED || psContext->eToken==ELSE || psContext->eToken==REM) break;

		if(psContext->eTokenType==QUOTE)
		{
			/*is string*/
			fprintf(psContext->apFileHandles[iFileNumber], psContext->acToken);
			len += strlen(psContext->acToken);
			last_delim = *psContext->acToken;
			get_token();
		}
		else if(psContext->eTokenType==STRINGVARIABLE)
		{

			/*is string*/
            pcTemp = get_StringVariable(psContext->acToken);
            fprintf(psContext->apFileHandles[iFileNumber], "%s", pcTemp);
		
            len += strlen(pcTemp);
			last_delim = *psContext->acToken;
			get_token();
		}
		else
		{
			/*is expression*/
			putback();
			eval_NumericExpression(&iAnswer, &fAnswer);
			last_delim = *psContext->acToken;
			get_token();

            /*avoid printing decimal point if whole number*/
            if ((fAnswer - (double)iAnswer) == 0)
            {
                len += fprintf(psContext->apFileHandles[iFileNumber],"%d", iAnswer);
            }
            else
            {
                len += fprintf(psContext->apFileHandles[iFileNumber],"%g", fAnswer);
            }

		}

		/*if comma, move to next tab stop*/
		if(*psContext->acToken==',')
		{
			/*compute number of spaces to move to next tab*/
			spaces = 8 - (len % 8);
			len += spaces;           /*add in the tabbing position*/
			while(spaces)
			{
				fprintf(psContext->apFileHandles[iFileNumber], " ");
				spaces--;
			}
		}
		else if(*psContext->acToken==';') fprintf(psContext->apFileHandles[iFileNumber], "");
		else if(*psContext->acToken=='+') fprintf(psContext->apFileHandles[iFileNumber], "");
		else if(psContext->eToken!=EOL && psContext->eToken!=FINISHED && psContext->eToken!=ELSE && psContext->eToken!=REM)
            basic_error(SYNTAX);

	} while (*psContext->acToken==';' || *psContext->acToken==',' || *psContext->acToken=='+');

	if(psContext->eToken==EOL || psContext->eToken==FINISHED || psContext->eToken==ELSE || psContext->eToken==REM)
	{
		if((last_delim != ';') && (last_delim!=',')) fprintf(psContext->apFileHandles[iFileNumber], "\n");
        putback();
	}
	else
	{
		basic_error(SYNTAX); /* error is not , or ; */
	}
}
#endif

/***************************************************************************
Function    :  basic_InputFromFile
Description :  Execute a simple form of the BASIC INPUT command.
Returns     :  Nothing
***************************************************************************/
void basic_InputFromFile(void)
{
	//char var;  // original and default signed on PC
    int var;     // Newman changed to int to avoid signed vs unsigned char problems
	int i;
    char acTextRead[256] = {0};
    int iItemsRead = 0;
    int iFileNumber;
    char *pcClosingQuote = NULL;
    char acTemp[1500];      //microsoft strikes again!  We need this since memmove is buggy

    /*Get File Number*/
    get_token();

	if ((psContext->eTokenType == INTEGERVARIABLE) || (psContext->eTokenType == FLOATVARIABLE))
    {
        putback();
        eval_IntegerExpression(&iFileNumber);
    }
    else
    {
        iFileNumber = *psContext->acToken - '0';
    }

    if ((iFileNumber < 0) || (iFileNumber > 10))
    {
        basic_printf("Error in INPUT#: File Number out of range\n");
        syntax_error(SYNTAX);
    }

    /*Check if that file number is already in use*/
    if (psContext->apFileHandles[iFileNumber] == NULL)
    {
        basic_printf("Error in INPUT#: File not open\n");
        syntax_error(SYNTAX);
    }

    /*Get the comma*/
    get_token();

    while (psContext->eToken!=EOL)
    {

	    /*Get variable name*/
        get_token();
        if (psContext->eToken == EOL) break;

        /*Get the basic_Input variable name*/
	    var = find_Variable(psContext->acToken, psContext->eTokenType);
        if (var == -1) var = create_Variable(psContext->acToken, psContext->eTokenType);
	    if(psContext->eTokenType == STRINGVARIABLE)
	    {
            iItemsRead = fscanf(psContext->apFileHandles[iFileNumber], "%s", psContext->asStringVariables[var].acValue);
		    if ((iItemsRead == 1) && (strlen(psContext->asStringVariables[var].acValue) > 0))
            {
                /*Erase comma if present.  Commas are optional separators.*/
                if (psContext->asStringVariables[var].acValue[strlen(psContext->asStringVariables[var].acValue)-1] == ',')
                {
                    psContext->asStringVariables[var].acValue[strlen(psContext->asStringVariables[var].acValue)-1] = 0;
                }

                /*Handle quoted strings.  Single quotes are supported.*/
                if (psContext->asStringVariables[var].acValue[0] == 0x27)
                {
                    /*Remove opening quote*/
                    //basic_printf("GOT OPENING QUOTE: %s\n", psContext->asStringVariables[var].acValue);

                    /*Do a memmove without using microsofts buggy function*/
                    //strcpy(acTemp, psContext->asStringVariables[var].acValue+1);
                    STRNCPY(acTemp, psContext->asStringVariables[var].acValue+1, sizeof(acTemp));
                    //strcpy(psContext->asStringVariables[var].acValue,acTemp);
                    STRNCPY(psContext->asStringVariables[var].acValue,acTemp, sizeof(psContext->asStringVariables[var].acValue));

                    //basic_printf("REMOVED OPENING QUOTE: %s\n", psContext->asStringVariables[var].acValue);

                    /*Check if closing quote was present*/
                    if ((pcClosingQuote = strrchr(psContext->asStringVariables[var].acValue, 0x27)) == NULL)
                    {

                        i = strlen(psContext->asStringVariables[var].acValue);
                        //if (i>0) i--;
                        iItemsRead = 0;

                        /*Keep reading until we get closing quote*/
                        while(iItemsRead != 0x27)
                        {

                            iItemsRead = fgetc(psContext->apFileHandles[iFileNumber]);

		                    if ((iItemsRead != EOF) && (iItemsRead != 0x27))
                            {
                                /*Add character to string*/
                                psContext->asStringVariables[var].acValue[i++] = iItemsRead;
                                psContext->asStringVariables[var].acValue[i] = 0;

                            }
                        }

                        /*Discard comma or delimiter*/
                        iItemsRead = fgetc(psContext->apFileHandles[iFileNumber]);
                    }
                    else
                    {
                        /*Remove quote*/
                        *pcClosingQuote = 0;
                    }
                }

            }
            else
            {
                //strcpy(psContext->asStringVariables[var].acValue, "EOF");  //indicates end of file
                STRNCPY(psContext->asStringVariables[var].acValue, "EOF", sizeof(psContext->asStringVariables[var].acValue));  //indicates end of file
            }
	    }
	    else
	    {
            /*Read Next item from the file*/
            iItemsRead = fscanf(psContext->apFileHandles[iFileNumber], "%d",&i);
		    if (iItemsRead == 1)
            {
                psContext->asIntegerVariables[var].iValue = i; /* store it */
            }
            else
            {
                printf("var = %d\n", var);
                psContext->asIntegerVariables[var].iValue = 0;   //indicates end of file
            }
	    }

        get_token();  //comma or EOL

        /*Ignore REM statement placed on end of INPUT# line*/
        if (strcasecmp(psContext->acToken,"REM") == 0)
        {
            basic_Rem();
            break;
        }
    }

} /*end Input_from_File*/


/***************************************************************************
Function    :  basic_Error
Description :  Printed when an unexpected keyword is found
Returns     :  Nothing
***************************************************************************/
void basic_Error(void)
{
    basic_printf("Unexpected keyword: %s\n", psContext->acToken);
    syntax_error(SYNTAX);
}


/***************************************************************************
Function    :  basic_Ignore
Description :  Do nothing
Returns     :  Nothing
***************************************************************************/
void basic_Ignore(void)
{
    basic_printf("Keyword ignored (%s)\n", psContext->acToken);
}


/***************************************************************************
Function    :  basic_End
Description :  Do nothing
Returns     :  Nothing
***************************************************************************/
void basic_End(void)
{
    get_token();

    /*Check for END IF statement*/
    if (psContext->eToken != IF)
    {
        /*Terminate the script*/
        putback();
		bScriptFileActive = 0;
    }
}



/***************************************************************************
Function    :  basic_Run
Description :  Runs a program.
Returns     :  Index of new context OR -1 if no instances of the interpreter
               remain
***************************************************************************/
void basic_Run(void)
{
    int iStatus = 0;
    char acProgramName[1500];
    int iIndex;
    char acTemp[1500];
	char acArguments[1500];
    int iTemp;

    /*Get Program name*/
    get_token();

    if (psContext->eTokenType == STRINGVARIABLE)
    {
        /*String Variable*/
        iIndex = find_Variable(psContext->acToken, STRINGVARIABLE);
        //strcpy(acProgramName, psContext->asStringVariables[iIndex].acValue);
        STRNCPY(acProgramName, psContext->asStringVariables[iIndex].acValue, sizeof(acProgramName));
    }
    else
    {
       /*Assume Literal String or quoted string*/
       //strcpy(acProgramName, psContext->acToken);
       STRNCPY(acProgramName, psContext->acToken, sizeof(acProgramName));
    }


    /*Assume all text up to the end of the line is to be passed to RUN*/
    acArguments[0] = 0;
	get_token();
	//basic_printf("tok = %s\n", psContext->acToken);	
	while((psContext->eToken != EOL) && (psContext->eToken != FINISHED))
	{

		strcat(acArguments, " ");

		/*Check for variable substitutions*/
		if(psContext->eTokenType==STRINGVARIABLE)
		{
			/*string variable*/
			strcat(acArguments, get_StringVariable(psContext->acToken));
		}
		else if(!isalpha(*psContext->acToken) || (psContext->eTokenType==QUOTE))
		{
			/*literal instance*/
			strcat(acArguments, psContext->acToken);
		}
		else
		{
			/*might be variable instance*/
			iIndex = find_Variable(psContext->acToken, INTEGERVARIABLE);
			if (iIndex != -1)
			{
				/*found a corresponding integer variable*/
                putback();
                eval_IntegerExpression(&iTemp);
				
                sprintf(acTemp, "%d", iTemp);
				strcat(acArguments, acTemp);
			}
			else
			{
				/*no such matching integer variable, so treat as literal string*/
				strcat(acArguments, psContext->acToken);
			}
		}

		get_token();
		//basic_printf("tok = %s\n", psContext->acToken);
	}


    /*Append arguments to filename*/
    if (strlen(acArguments) > 0)
    {
        //basic_printf("appending %d chars. string = %s\n", strlen(acArguments), acArguments);
        strcat(acProgramName, acArguments);
    }
    
    //basic_printf("Run String: %s\n", acProgramName);
    
    /*Recursive call of the BASIC interpreter*/
    // TODO -- readScriptFile(acProgramName);
    basic_printf("Not implemented.  You must specify a file name.\n");
}


/***************************************************************************
Function    :  basic_Beep
Description :  Make a noise!
Returns     :  nothing
***************************************************************************/
void basic_Beep(void)
{
    int iStatus = 0;
    int iFrequency = 16000;
    int iDuration = 400;


    /*Get opening bracket*/
    get_Bracket('(');

    /*Get Frequency name*/
    get_token();

    switch(psContext->eTokenType)
    {

    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &iFrequency);
        break;

//    case NUMBER:
//        sscanf(psContext->acToken, "%d", &iFrequency);
//        break;

    case DELIMITER:
        putback();
        break;

    default:
        putback();
        eval_IntegerExpression(&iFrequency);
        break;

    }

    /*Get Comma*/
    get_token();

    /*Get Duration*/
    get_token();

    switch(psContext->eTokenType)
    {

    case STRINGVARIABLE:
        sscanf(get_StringVariable(psContext->acToken), "%d", &iDuration);
        break;

    case DELIMITER:
        putback();
        break;

    default:
        putback();
        eval_IntegerExpression(&iDuration);
        break;
    }
    
    get_Bracket(')');

    /*"Come on, Feel the noise"*/
    //Beep(iFrequency, iDuration);
    basic_printf("BEEP!\n\a");

}




/***************************************************************************
Function    :  basic_NotImplemented
Description :  
Returns     :  
***************************************************************************/
void basic_NotImplemented(void)
{
    syntax_error(UNIMPLEMENTED);
}



/***************************************************************************
Function    :  basic_Shared
Description :  Adds to the list of common or shared variables.
Returns     :  Nothing
***************************************************************************/
void basic_Shared(void)
{
    int iIndex;


    while((psContext->eToken != EOL) && (psContext->eTokenType != FINISHED))
    {
        /*get variable name*/
        get_token();
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

        /*Store variable name*/
        for(iIndex = 0; iIndex < NUM_SHARED_VARIABLES; iIndex++)
        {
            if (strcmp(psContext->acSharedVariables[iIndex], "UNUSED SHARED VARIABLE") == 0)
            {
                //strcpy(psContext->acSharedVariables[iIndex], psContext->acToken);
                STRNCPY(psContext->acSharedVariables[iIndex], psContext->acToken, sizeof(psContext->acSharedVariables[iIndex]));
                break;
            }
        }
        if (iIndex == NUM_SHARED_VARIABLES)
        {
            basic_printf("Error: out of memory for shared/common variable\n");
            syntax_error(SYNTAX);
        }

        /*get comma*/
        if (get_token() != DELIMITER) break; //comma
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

    }

}


/***************************************************************************
Function    :  basic_Common
Description :  Adds to the list of common or shared variables.
Returns     :  Nothing
***************************************************************************/
void basic_Common(void)
{
    int iIndex;


    while((psContext->eToken != EOL) && (psContext->eTokenType != FINISHED))
    {
        /*get variable name*/
        get_token();
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

        /*Store variable name*/
        for(iIndex = 0; iIndex < NUM_COMMON_VARIABLES; iIndex++)
        {
            if (strcmp(psContext->acCommonVariables[iIndex], "UNUSED COMMON VARIABLE") == 0)
            {
                //strcpy(psContext->acCommonVariables[iIndex], psContext->acToken);
                STRNCPY(psContext->acCommonVariables[iIndex], psContext->acToken, sizeof(psContext->acCommonVariables[iIndex]));
                break;
            }
        }
        if (iIndex == NUM_COMMON_VARIABLES)
        {
            basic_printf("Error: out of memory for common variable\n");
            syntax_error(SYNTAX);
        }

        /*get comma*/
        if (get_token() != DELIMITER) break; //comma
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

    }

}


/***************************************************************************
Function    :  basic_Len
Description :  Len source$ length where source$ is a string and length is the 
               integer variable that will hold the string length
Returns     :  Nothing
***************************************************************************/
void basic_Len(void)
{
    char *source = NULL;
    int iIndex = 0;
	char acTemp[3000];


    /*source$*/
	get_token();
	string_atom(acTemp, sizeof(acTemp));
	source = acTemp;
	

    /*length*/
    get_token();
    if((psContext->eTokenType !=INTEGERVARIABLE) && (psContext->eTokenType !=FLOATVARIABLE))
	{
        syntax_error(SYNTAX);
	}

    /*Find or create the destination variable*/
    iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

    /*Get Length*/
    switch (psContext->eTokenType)
    {
    case INTEGERVARIABLE:
        psContext->asIntegerVariables[iIndex].iValue = strlen(source);
        break;
    case FLOATVARIABLE:
        psContext->asFloatVariables[iIndex].fValue = strlen(source);
        break;
    default:
        basic_printf("BASIC Internal error in LEN\n");
        break;
    }
}


/***************************************************************************
Function    :  basic_Cls
Description :  Clear screen
Returns     :  Nothing
***************************************************************************/
void basic_Cls(void)
{

    // static HANDLE hConsoleScreenBuffer = INVALID_HANDLE_VALUE;  
    // DWORD cWritten; 
    // BOOL fSuccess; 
    // COORD coord;  
    // CHAR chFillChar; 

           
    // /*Get Console Ouput Buffer handle*/
    // if (hConsoleScreenBuffer == INVALID_HANDLE_VALUE)
    // {
    //     hConsoleScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE); // memory leak!
    // }

    // /*Clear screen*/
    // if (hConsoleScreenBuffer != INVALID_HANDLE_VALUE)
    // {
    //     // Fill an 80-by-50-character screen buffer with the space character. 
    //     coord.X = 0;            // start at first cell 
    //     coord.Y = 0;            //   of first row 
    //     chFillChar = ' '; 
 
    //     fSuccess = FillConsoleOutputCharacter( 
    //                hConsoleScreenBuffer,           // screen buffer handle 
    //                chFillChar,                     // fill with spaces 
    //                80*3000,                        // number of cells to fill 
    //                coord,                          // first cell to write to 
    //                &cWritten);                     // actual number written
        

    //     fSuccess = FillConsoleOutputAttribute(
    //                hConsoleScreenBuffer,           // screen buffer handle 
    //                FOREGROUND_RED |
    //                FOREGROUND_GREEN |
    //                FOREGROUND_BLUE,                // color attribute to write
    //                80*3000,                        // number of cells to fill 
    //                coord,                          // first cell to write to 
    //                &cWritten);  

    //     SetConsoleCursorPosition(hConsoleScreenBuffer, coord);
    
    // }  
 
} 


/***************************************************************************
Function    :  basic_SaveScreen
Description :  Save a block of the console for later restoration
Returns     :  Nothing
***************************************************************************/
void basic_SaveScreen(void)
{

//     static HANDLE hConsoleScreenBuffer = INVALID_HANDLE_VALUE;  
// //    DWORD cWritten; 
//     BOOL fSuccess; 
//     COORD coord; 
//     COORD buf_size;
//     //PCHAR_INFO lpBuffer;    
//     SMALL_RECT ReadRegion;
           
//     /*Get Console Ouput Buffer handle*/
//     if (hConsoleScreenBuffer == INVALID_HANDLE_VALUE)
//     {
//         hConsoleScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE);   //memory leak!
//     }

//     /*Save screen*/
//     if (hConsoleScreenBuffer != INVALID_HANDLE_VALUE)
//     {
//         // Fill an 80-by-50-character screen buffer with the space character. 
//         coord.X = 0;            // start at first cell 
//         coord.Y = 0;            //   of first row  
//         buf_size.X = 200;
//         buf_size.Y = 200;
        
//         ReadRegion.Top = 0;
//         ReadRegion.Bottom = 50;
//         ReadRegion.Left = 0;
//         ReadRegion.Right = 80;

//         fSuccess = ReadConsoleOutput(
//                    hConsoleScreenBuffer,    // handle to a console screen buffer
//                    ConsoleBuffer,           // address of buffer that receives data
//                    buf_size,                // column-row size of destination buffer
//                    coord,                   // upper-left cell to write to
//                    &ReadRegion              // address of rectangle to read from
//                    ); 
//         //basic_printf("ReadConsoleOutput returned %d Err %d\n", fSuccess, GetLastError());        
//     }  
 
} 

/***************************************************************************
Function    :  basic_RestoreScreen
Description :  Restore a block of the console test that was saved previously
Returns     :  Nothing
***************************************************************************/
void basic_RestoreScreen(void)
{

//     static HANDLE hConsoleScreenBuffer = INVALID_HANDLE_VALUE;  
// //    DWORD cWritten; 
//     BOOL fSuccess; 
//     COORD coord;
//     COORD buf_size;    
//     //PCHAR_INFO lpBuffer;    
//     SMALL_RECT ReadRegion;
           
//     /*Get Console Ouput Buffer handle*/
//     if (hConsoleScreenBuffer == INVALID_HANDLE_VALUE)
//     {
//         hConsoleScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE);   //memory leak!
//     }

//     /*Restore screen*/
//     if (hConsoleScreenBuffer != INVALID_HANDLE_VALUE)
//     {
//         // Fill an 80-by-50-character screen buffer with the space character. 
//         coord.X = 0;            // start at first cell 
//         coord.Y = 0;            //   of first row  
//         buf_size.X = 200;
//         buf_size.Y = 200;
        
//         ReadRegion.Top = 0;
//         ReadRegion.Bottom = 50;
//         ReadRegion.Left = 0;
//         ReadRegion.Right = 80;

//         fSuccess = WriteConsoleOutput(
//                    hConsoleScreenBuffer,    // handle to a console screen buffer
//                    ConsoleBuffer,           // address of buffer that receives data
//                    buf_size,                // column-row size of destination buffer
//                    coord,                   // upper-left cell to write to
//                    &ReadRegion              // address of rectangle to read from
//                    );         
//         //basic_printf("WriteConsoleOutput returned %d Err %d\n", fSuccess, GetLastError());
//     }  
 
}


/***************************************************************************
Function    :  basic_CursorXY
Description :  Move cursor to screen co-ordinates
Returns     :  Nothing
***************************************************************************/
void basic_CursorXY(void)
{

    // static HANDLE hConsoleScreenBuffer = INVALID_HANDLE_VALUE;   
    // COORD coord;  
    // int x = 0;
    // int y = 0;

             
    // get_Bracket('(');                       // opening bracket
    // eval_IntegerExpression(&x);             // X co-ordinate
    // if (get_token() != DELIMITER)
    // {
    //     basic_error(SYNTAX);                     //comma
    // }
    // eval_IntegerExpression(&y);             // Y co-ordinate
    // get_Bracket(')');                       // closing bracket

    // coord.X = (short)x;           
    // coord.Y = (short)y; 

    // /*Get Console Ouput Buffer handle*/
    // if (hConsoleScreenBuffer == INVALID_HANDLE_VALUE)
    // {
    //     hConsoleScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    // }

    // /*Clear screen*/
    // if (hConsoleScreenBuffer != INVALID_HANDLE_VALUE)
    // {
    //     // Fill an 80-by-50-character screen buffer with the space character. 
    //     SetConsoleCursorPosition(hConsoleScreenBuffer, coord);
    
    // }  
 
}


/***************************************************************************
Function    :  basic_Sleep
Description :  Wait for an event of interest
                  - at present this is only keyboard input
Returns     :  Nothing
***************************************************************************/
void basic_Sleep(void)
{
    double fValue = 0;
    int sleep_seconds = 0;

    /*Get sleep value*/
    eval_FloatExpression(&fValue);
    sleep_seconds = (int)fValue;

    // sleep but pat the watchdog occasionally
    do
    {
        if (sleep_seconds > 30)
        {
            SLEEP_MS(30*1000);
            sleep_seconds -=30;
            
            hc_pat_watchdog();
        }
        else
        {
            SLEEP_MS(sleep_seconds*1000);
            sleep_seconds = 0;
        }
    } while (sleep_seconds > 0);
    
}



/***************************************************************************
Function    :  basic_Dim
Description :  Execute a DIM statement.
Returns     :  Nothing
***************************************************************************/
void basic_Dim(void)
{
    int iIndex;
    int iDim;
    int iArraySize;
    int iUnitSize;
    int iNumDim;
    int j;

    while(1)
    {
        // get variable name
        get_token();
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }

        switch(psContext->eTokenType)
        {
        case STRINGVARIABLE:
            iUnitSize = iArraySize = STRING_VAR_LEN;
            break;

        case INTEGERVARIABLE:
            iUnitSize= iArraySize = sizeof(int);
            break;

        case FLOATVARIABLE:
            iUnitSize = iArraySize = sizeof(double);
            break;

        default:
            basic_printf("Error: Expected a variable name!\n");
            syntax_error(SYNTAX);
            break;
        }

        // scan to see if it exists already
        for(iIndex=0; iIndex < NUM_ARRAY_VARIABLES; iIndex++)
        {
            if (strcmp(psContext->acToken, psContext->asArrayVariables[iIndex].acName) == 0)
            {
                basic_printf("Runtime Error: Attempted to redimension array %s\n", psContext->acToken);
                syntax_error(SYNTAX);
            }
        }

        // find a free array variable
        for(iIndex=0; iIndex < NUM_ARRAY_VARIABLES; iIndex++)
        {
            if (psContext->asArrayVariables[iIndex].pcValue == NULL)
            {
                //strcpy(psContext->asArrayVariables[iIndex].acName, psContext->acToken);
                STRNCPY(psContext->asArrayVariables[iIndex].acName, psContext->acToken, sizeof(psContext->asArrayVariables[iIndex].acName));
                break;
            }
        }
        
        if (iIndex >= NUM_ARRAY_VARIABLES)
        {
            basic_printf("Runtime Error: Too many array variables\n");
            syntax_error(SYNTAX);
        }

        get_Bracket('(');

        // zero dimensions
        for (iDim=0; iDim < MAX_ARRAY_DIM; iDim++)
        {
            psContext->asArrayVariables[iIndex].iDimensions[iDim] = 0;
            psContext->asArrayVariables[iIndex].iMultiplier[iDim] = 0;
        }

        // store dimensions
        for (iDim=0; iDim < MAX_ARRAY_DIM; iDim++)
        {
            eval_IntegerExpression(&psContext->asArrayVariables[iIndex].iDimensions[iDim]);
            //basic_printf("%s Array dimension %d size is %d\n", psContext->asArrayVariables[iIndex].acName, iDim, psContext->asArrayVariables[iIndex].iDimensions[iDim]);

            if (psContext->asArrayVariables[iIndex].iDimensions[iDim] < 1)
            {
                basic_printf("Error: invalid dimension size %d\n", psContext->asArrayVariables[iIndex].iDimensions[iDim]);
                syntax_error(SYNTAX);
            }

            iArraySize *= psContext->asArrayVariables[iIndex].iDimensions[iDim];
            //basic_printf("Array size is %d\n", iArraySize);

            if (get_token() != DELIMITER)
            {
                basic_printf("Error: expected comma or closing bracket\n");
                syntax_error(SYNTAX);
            }


            if (psContext->acToken[0] == ')') break;
        }

        if (psContext->acToken[0] != ')')
        {
            get_Bracket(')');
        }        

        // calculate multiplier for each dimension
        for (iNumDim=MAX_ARRAY_DIM-1; psContext->asArrayVariables[iIndex].iDimensions[iNumDim] == 0; iNumDim--);
        
        if (iNumDim++ < 0)
        {
            basic_printf("No dimensions defined\n");
            syntax_error(SYNTAX); 
        }

        for(iDim=0; iDim < iNumDim; iDim++)
        {
            psContext->asArrayVariables[iIndex].iMultiplier[iDim] = iUnitSize;
            
            for(j=iDim+1; j < iNumDim; j++)
            {
                psContext->asArrayVariables[iIndex].iMultiplier[iDim] *= psContext->asArrayVariables[iIndex].iDimensions[j];    
            }
        }

        // allocate memory for the array
        psContext->asArrayVariables[iIndex].pcValue = malloc(iArraySize);
        if (psContext->asArrayVariables[iIndex].pcValue == NULL)
        {
            basic_printf("Error: out of memory during array creation (size %d bytes)\n", iArraySize);
            syntax_error(SYNTAX);
        }

        // zero array
        for(j=0; j<iArraySize; j++)
        {            
            psContext->asArrayVariables[iIndex].pcValue[j] = 0;
        }

        // get comma
        if (get_token() != DELIMITER) break; //comma
        if (psContext->eToken == EOL)
        {
            putback();
            break;
        }
    }
}

