/******************************************************************************
File:		basic_interpretor
Description:A BASIC interpreter with langauge extensions for SNMP and automatic
            testing.  This file contains the main line of the interpreter.
******************************************************************************/
//Use this define to enable parent scripts to continue after a child has crashed
//#define PARENTS_CONTINUE_AFTER_ERROR

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


/*External Variables*/
// extern char acSnmpIp[];                     //SNMP's own IP address
// extern char acIpAddress[];                  //Target Agent's IP address
// extern char acContextId[20];                //Target Agent's context ID

 
/*Public Variable*/
char bScriptFileActive = 0;                 //indicates a script is running
char bSteppingActive = 0;                   //script file is executed one line at a time
char bTraceActive = 0;                      //script file is executed one line at a time
int bTerminateWithExtremePrejudice = 0;     //script has been terminated by user pressing ESCAPE
bool syntax_error_occured = false;

/*Private Variables*/
int iContextIndex = -1;
tsBasicContext *psContext;                  //current BASIC context
int bClearInkey = 0;                        //flag used to indicate BASIC has read the keystroke
// tsBasicContext *apsContextStack[BASIC_RECURSION_DEPTH] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
//                                                           NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
tsBasicContext *apsContextStack[BASIC_RECURSION_DEPTH] = {NULL};
static bool context_initialized = false;       // used to persist context in interactive shell

/*Local Prototypes*/
void display_ScriptLine(void);
void basic_Stop(void);

/*BASIC command lookup table*/
tsCommand asCommandTable[] =
{
    /*Command name must be lowercase in this table.*/   
    "abs",              ABS,                basic_NotImplemented,
    //"asc",              ASC,                basic_NotImplemented,
    "atoi",             ATOI,               basic_AtoI,
    "atoihex",          ATOIHEX,            basic_AtoIHex,
    "beep",             BEEP,               basic_Beep,
    "call",             CALL,               basic_NotImplemented,
    "chain",            CHAIN,              basic_Chain,
    "clear",            CLEAR,              basic_NotImplemented,
    "close",            CLOSE,              basic_Close,
    "cls",              CLS,                basic_Cls,
    "colour",           COLOUR,             basic_Colour,
    "common",           COMMON,             basic_Common,
    "com",              COM,                basic_Com,
    "contextid",        CONTEXTID,          basic_ContextId,
    "cursorxy",         CURSORXY,           basic_CursorXY,    
    "delay",            DELAY,              basic_Delay,
    "dim",              DIM,                basic_Dim,
    "dump",             DUMP,               dump_Variables,
    "else",             ELSE,               basic_Else,
    "elseif",           ELSEIF,             basic_Else,
    "end",              END,                basic_End,
    "for",              FOR,                basic_For,
    "function",         DEF_FUNC,           basic_Function,
    "get",              GET,                basic_Get,
    "getnext",          GETNEXT,            basic_GetNext,
    "gosub",            GOSUB,              basic_Gosub,
    "goto",             GOTO,               basic_Goto,
    "gpib",             GPIB_CMD,           basic_Gpib,
    "graph",            GRAPH,              basic_Graph,
    "hex$",             HEX$,               basic_NotImplemented,
    "home",             HOME,               basic_Home,
    "if",               IF,                 basic_If,
    "input",            INPUT,              basic_Input,
    "input#",           INPUT_FROM_FILE,    basic_InputFromFile,
    "instr",            INSTR,              basic_NotImplemented,
    "interchardelay",   INTERCHARDEALY,     basic_InterCharDelay,    
    "itoahex",          ITOAHEX,            basic_ItoAHex,
    "left$",            LEFT$,              basic_Left,
    "len",              LEN,                basic_Len,
    "let",              LET,                basic_Let,
    "log",              LOG,                basic_LogToFile,
    "merge",            MERGE,              basic_NotImplemented,
    "mid$",             MID,                basic_Mid,
    "next",             NEXT,               basic_Next,
    "on",               ON,                 basic_On,
    "open",             OPEN,               basic_Open,
    "plot",             PLOT,               basic_Plot,
    "print",            PRINT,              basic_Print,
    "print#",           PRINT_TO_FILE,      basic_PrintToFile,
    "quiet",            QUIET,              basic_Quiet,
    "rasaddr",          RASADDR,            basic_RasAddr,
    "relay",            RELAY,              basic_Relay,
    "rem",              REM,                basic_Rem,
    "return",           RETURN,             basic_Return,
	"retry",            RETRY,              basic_Retry,
    "right$",           RIGHT$,             basic_Right,
    "run",              RUN,                basic_Run,
    "screen_save",      SCREEN_SAVE,        basic_SaveScreen,
    "screen_restore",   SCREEN_RESTORE,     basic_RestoreScreen,
    "set",              SET,                basic_Set,
    "shared",           SHARED,             basic_Shared,
    "shelly_get",       SHELLY_GET,         basic_ShellyGet,
    "shelly_switch",    SHELLY_SWITCH,      basic_ShellySwitch,    
    "sleep",            SLEEP,              basic_Sleep,
    "step",             STEP,               basic_Error,
    "stop",             STOP,               basic_Stop,
    "str$",             STR$,               basic_NotImplemented,
    "sub",              SUB,                basic_NotImplemented,
    "system",           SYSTEM,             basic_System,
    "target",           TARGET,             basic_Target,
    "terminal",         TERMINAL,           basic_Terminal,
    "tftp",             TFTP,               basic_Tftp,
    "then",             THEN,               basic_Error,
    "timeout",          TIMEOUT,            basic_TimeOut,
    "timestamp",        TIMESTAMP,          basic_TimeStamp,
    "to",               TO,                 basic_Error,
    "traps",            TRAPS,              basic_TrapLog,
    "ucase$",           UCASE$,             basic_Ucase,
    "val",              VAL,                basic_NotImplemented,
    "verbose",          VERBOSE,            basic_Verbose,
    "wend",             WEND,               basic_Wend,
    "while",            WHILE,              basic_While,

    /*Insert new commands above this line*/
    "",                 END,                basic_Ignore    
};

/***************************************************************************
Function    :  basic_Interpreter
Description :  Launchs a new instance of the BASIC interpreter and executes
               the BASIC program contained in the file pcFileName.
Returns     :  0 - OK
               1 - error initialising BASIC interpreter
***************************************************************************/
int basic_Interpreter(char *pcFileName, char *pcArguments, char *program_in_memory, int len_program_in_memory, bool reset_context)
{
	int x;
	int iKey;
    int iInkeyIndex;

    if (reset_context || !context_initialized)
    {
        /*Create a new BASIC Context*/
        basic_CreateContext(pcFileName, pcArguments);
    }

    if (program_in_memory)
    {
        /*Copy the program to execute from RAM*/
        load_program_from_ram(psContext->pcProgram, program_in_memory, len_program_in_memory);  //TODO: this will change if we allow entering programs line by line
        printf("PROGRAM BEGIN\n%s\nPROGRAM END\n", psContext->pcProgram);
    }
	/*Load the program to execute from file*/
	else if(!load_program(psContext->pcProgramCounter, pcFileName))
	{
		basic_printf("Could not open BASIC script file\n");
       
        /*Erase the current context*/
        basic_DestroyContext();

        /*Check for nested BASIC programs*/
        // if (iContextIndex >=0) ORIGINAL
        if (iContextIndex >0)  // Newman altered 2026-06-19        
        {
            /*Reactivate the parent program*/
            bScriptFileActive = 1;
        }

		return(1);
	}

    /*Reload Trace Window*/
    if (bTraceActive)
    {
        //sock_TraceOpen(psContext->acFileName);
    }

	/*The code in the if block only executes if an error occurs*/
//     if(setjmp(psContext->sEnviroment))
//     {
        
//         /*Erase the current context*/
//         basic_DestroyContext();

// #ifdef PARENTS_CONTINUE_AFTER_ERROR
//         //THIS CODE controls whether parent scripts are terminated
//         //when a child script encounters an error
        
//         /*Check for nested BASIC programs*/
//         if (iContextIndex >=0)
//         {
//             /*Acitvate the parent program*/
//             bScriptFileActive = 1;
//         }
// #endif
        
//         return(1); 
//     }

	scan_labels();                           /*find the labels in the program*/
    scan_UserFunctions();                    /*find the functions in the program*/
	psContext->iTopOfForStack = 0;           /*initialize the FOR stack index*/
	psContext->iTopOfWhileStack = 0;         /*initialize the WHILE stack index*/
	psContext->iTopOfGosubStack = 0;         /*initialize the GOSUB stack index*/


    /*Remember index of INKEY$ variable to quickly store last keystroke*/
    iInkeyIndex = find_Variable("inkey$", STRINGVARIABLE);
    if (iInkeyIndex == -1) iInkeyIndex = create_Variable("inkey$", STRINGVARIABLE);


	/*Execute Script*/
	bScriptFileActive = 1;
	do
	{
		//x = 0;

        /*Get the next token*/
        psContext->eTokenType = get_token();

        //basic_printf("Token String = %s Token = %d Type = %d\n", psContext->acToken, psContext->eToken, psContext->eTokenType); 


		/*Check for assignment statement*/
		if((psContext->eTokenType==INTEGERVARIABLE) ||
		   (psContext->eTokenType==FLOATVARIABLE) ||
           (psContext->eTokenType==STRINGVARIABLE))
		{
			/*This is a LET statement without the optional LET keyword*/
            putback();
            if (bSteppingActive) basic_Stop();
			basic_Let();        
		}
        else if (psContext->eTokenType==USERFUNCTION)
        {
            /*Trace the function call*/
            if (bSteppingActive)
            {
                putback();
                basic_Stop();
                get_token();
            }

            basic_Function();            
        }
		else
		{
            /*Assume it is a command*/
			for(x= 0; x < sizeof(asCommandTable)/sizeof(asCommandTable[0]); x++)
            {
                if (psContext->eToken == asCommandTable[x].eToken)
                {
                    /*Trace the command*/
                    if ((bSteppingActive) &&
                        (psContext->eToken != REM) &&
                        (psContext->eToken != STOP))
                    {
                        putback();
                        basic_Stop();
                        get_token();
                    }

                    /*Perform the command*/
                    asCommandTable[x].pfCommand();
                    break;
                }
            }            

		} /*end else*/


		/*Check if user has pressed ESCAPE*/
		// if (kbhit())
		// {
		// 	iKey = getch();
		// 	if ((iKey == 0x00) || (iKey == 0xE0))
		// 	{
		// 		/*Handle Extended Keys*/
		// 		iKey = getch() + 256;
		// 	}

		// 	if (iKey == ESC)
		// 	{
		// 		/*Terminate request*/
		// 		bScriptFileActive = 0;

        //         /*Force termination of all scripts*/
        //         bTerminateWithExtremePrejudice = 1;        

		// 	}

        //     /*Update INKEY$ value for BASIC scripts to read*/
        //     sprintf(psContext->asStringVariables[iInkeyIndex].acValue, "%c",iKey);
		// }
        // else if (bClearInkey == 1)
        // {
        //     /*Clear INKEY$ since it has been read by the BASIC script*/
        //     sprintf(psContext->asStringVariables[iInkeyIndex].acValue, "");
        //     bClearInkey = 0;
        // }


		/*Check for terminate request. END, INPUT or ESCAPE*/
		if (!bScriptFileActive) break;

       hc_pat_watchdog();

	} while (psContext->eToken != FINISHED);

    if (reset_context || syntax_error_occured)   // temporary hack to clean up interactive shell after syntax error
    {
        /*Erase the current context*/
        basic_DestroyContext();
        syntax_error_occured = false;
    }

    /*Check for nested BASIC programs*/
    if (iContextIndex >=0)        // TODO why = 0???
    {
        /*If user pressed ESCAPE do not reactivate parent program*/
        if (!bTerminateWithExtremePrejudice)
        {
            /*Reactivate the parent program*/
            bScriptFileActive = 1;
        }

        /*Reload Trace Window*/
        if (bTraceActive)
        {
            //sock_TraceOpen(psContext->acFileName);
        }
    }
    else
    {
        /*Disable Stepping*/
        bSteppingActive = 0;
        
        if (bTraceActive)
        {
            /*Close the trace Window*/
            //sock_TraceHide();
            bTraceActive = 0;
        }

        /*Disable forced termination of all scripts*/
        bTerminateWithExtremePrejudice = 0;        
    }

	return 0;
}

/***************************************************************************
Function    :  load_program
Description :  Load a program.
Returns     :  1 = Rock and Roll
***************************************************************************/
int load_program(char *p, char *fname)
{
    FILE *fp;
    int i=0;
    const char acProgramTerminator[] = "\r\nEND";
    char *program = NULL;
    
    program = p;

    if(!(fp=fopen(fname, "rb"))) return 0;

    i = 0;
    do
    {
        *p = getc(fp);
        p++; i++;
    } while(!feof(fp) && i<PROG_SIZE);

    /*Terminate the program with an END statement*/
    if (i < (PROG_SIZE - sizeof(acProgramTerminator)))
    {
        strcat(program, acProgramTerminator); 
    }
  
    fclose(fp);
    
    return 1;
}

/***************************************************************************
Function    :  load_program_from_ram
Description :  Load a program.
Returns     :  1 = Rock and Roll
***************************************************************************/
int load_program_from_ram(char *p, char *s, int len)
{
    int i=0;
    const char acProgramTerminator[] = "\r\nEND\r\n";
    char *program = NULL;

    program = p;

    i = 0;
    do
    {
        *p = *s;
        p++; s++; i++;
    } while((i<len) && (i<PROG_SIZE));

    /*Terminate the program with an END statement*/
    if (i < (PROG_SIZE - sizeof(acProgramTerminator)))
    {
        strcat(program, acProgramTerminator);
    }
  
    STRNCPY(psContext->acFileName, "command line", sizeof(psContext->acFileName));
    psContext->pcProgramCounter = psContext->pcProgram;

    return 1;
}

/***************************************************************************
Function    :  find_eol
Description :  Find the start of the next line.
Returns     :  Nothing
***************************************************************************/
void find_eol(void)
{
  while((*psContext->pcProgramCounter!='\n')  && (*psContext->pcProgramCounter!='\0'))
  {
      ++psContext->pcProgramCounter;
  }
  if(*psContext->pcProgramCounter) psContext->pcProgramCounter++;
}


/***************************************************************************
Function    :  scan_labels
Description :  Find all labels.
Returns     :  Nothing
***************************************************************************/
void scan_labels(void)
{
    int addr;
    char *pcTempProgramCounter;
    teToken eTempToken;
    teTokenType eTempTokenType;

     // zero all labels
    label_init(); 

    // save
    pcTempProgramCounter = psContext->pcProgramCounter;   
    eTempToken = psContext->eToken;
    eTempTokenType = psContext->eTokenType;


    
    // if the first token in the file is a label
    get_token();
    if( (psContext->eTokenType==NUMBER) || (psContext->eTokenType==LABEL) )
    {
        //strcpy(psContext->sLabelTable[0].name, psContext->acToken);
        STRNCPY(psContext->sLabelTable[0].name, psContext->acToken, sizeof(psContext->sLabelTable[0].name));
        psContext->sLabelTable[0].p = psContext->pcProgramCounter;        
    }

    find_eol();
    do
    {
        get_token();
	    if( (psContext->eTokenType==NUMBER) || (psContext->eTokenType==LABEL) )
	    {
	        addr = get_next_label(psContext->acToken);
            if(addr==-1 || addr==-2)
	        {
                (addr==-1) ? syntax_error(LAB_TAB_FULL):syntax_error(DUP_LAB);
            }
            //strcpy(psContext->sLabelTable[addr].name, psContext->acToken);
            STRNCPY(psContext->sLabelTable[addr].name, psContext->acToken, sizeof(psContext->sLabelTable[addr].name));
            
            /* save current location in program */
            psContext->sLabelTable[addr].p = psContext->pcProgramCounter;
        }
        
        /* if not on a blank line, find basic_Next line */
        if(psContext->eToken!=EOL) find_eol();

    } while(psContext->eToken!=FINISHED);

    // restore
    psContext->pcProgramCounter = pcTempProgramCounter;  
    psContext->eToken = eTempToken;
    psContext->eTokenType = eTempTokenType;
}


/***************************************************************************
Function    :  get_next_label
Description :  Return index of next free position in label array.
Returns     :  -1 is returned if the array is full.
               -2 is returned when duplicate label is found.
***************************************************************************/
int get_next_label(char *s)
{
    int t;

    for(t=0;t<NUM_LAB;++t)
    {
        if(psContext->sLabelTable[t].name[0]==0) return t;
        if(!strcmp(psContext->sLabelTable[t].name,s)) return -2; /* duplicate ! */
    }

  return -1;
}

/***************************************************************************
Function    :  find_label
Description :  Find location of given label.
Returns     :  Null if label is not found;
               otherwise a pointer to the position of the label is returned.
***************************************************************************/
char *find_label(char *s)
{
  int t;

  for(t=0; t<NUM_LAB; ++t)
    if(!strcmp(psContext->sLabelTable[t].name,s)) return psContext->sLabelTable[t].p;
  return(NULL); /* error condition */
}


/***************************************************************************
Function    :  label_init
Description :  Initialize the array that holds the labels.
               By convention, a null label name indicates that array
               position is unused.
Returns     :  Nothing
***************************************************************************/
void label_init(void)
{
  int t;

  for(t=0; t<NUM_LAB; ++t) psContext->sLabelTable[t].name[0]='\0';
}

/***************************************************************************
Function    :  scan_UserFunctions
Description :  Find all functions.
Returns     :  Nothing
***************************************************************************/
void scan_UserFunctions(void)
{
    int addr;
    char *pcTempProgramCounter;
    teToken eTempToken;
    teTokenType eTempTokenType;
    int iParamNum;
    int iIndex;

     // zero all functions
    userFunction_init(); 

    // save
    pcTempProgramCounter = psContext->pcProgramCounter;   
    eTempToken = psContext->eToken;
    eTempTokenType = psContext->eTokenType;


    do
    {
        get_token();
        //basic_printf("Token String = %s Token = %d Type = %d\n", psContext->acToken, psContext->eToken, psContext->eTokenType); 

	    if(psContext->eToken==DEF_FUNC)
	    {
            get_token();
            //basic_printf("FOUND FUNCTION: %s\n", psContext->acToken);
	        addr = get_next_UserFunction(psContext->acToken);
            if(addr==-1 || addr==-2)
	        {
                (addr==-1) ? syntax_error(FUNC_TAB_FULL):syntax_error(DUP_FUNC);
            }
            //strcpy(psContext->sUserFunctionTable[addr].name, psContext->acToken);
            STRNCPY(psContext->sUserFunctionTable[addr].name, psContext->acToken, sizeof(psContext->sUserFunctionTable[addr].name));
            

        // find the parameters
        get_Bracket('(');
        for (iParamNum=0; iParamNum < 10; iParamNum++)
        {
            get_token();
            if (psContext->acToken[0] == ',') get_token();
            if (psContext->acToken[0] == ')') break;

            psContext->sUserFunctionTable[addr].asParameters[iParamNum].eParamType = psContext->eTokenType;

            switch(psContext->eTokenType)
            {
            case INTEGERVARIABLE:
                //strcpy(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamInt.acName, psContext->acToken);
                STRNCPY(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamInt.acName, psContext->acToken, sizeof(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamInt.acName));
                psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamInt.iValue = 0;
                break;

            case FLOATVARIABLE:
                //strcpy(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamFlt.acName, psContext->acToken);
                STRNCPY(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamFlt.acName, psContext->acToken, sizeof(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamFlt.acName));
                psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamFlt.fValue = 0;
                break;

            case STRINGVARIABLE:
                //strcpy(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamStr.acName, psContext->acToken);
                STRNCPY(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamStr.acName, psContext->acToken, sizeof(psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamStr.acName));
                psContext->sUserFunctionTable[addr].asParameters[iParamNum].sParamStr.acValue[0] = 0;
                break;

            default:
                syntax_error(SYNTAX);
                break;
            }

                iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
                if (iIndex == -1) iIndex = create_Variable(psContext->acToken, psContext->eTokenType);

            }

            for (; iParamNum < 10; iParamNum++)
            {
                psContext->sUserFunctionTable[addr].asParameters[iParamNum].eParamType = DELIMITER;
            }


            /* save current location in program */
            psContext->sUserFunctionTable[addr].p = psContext->pcProgramCounter;
        }
        
        /* if not on a blank line, find next line */
        if(psContext->eToken!=EOL) find_eol();

    } while(psContext->eToken!=FINISHED);

    // restore
    psContext->pcProgramCounter = pcTempProgramCounter;  
    psContext->eToken = eTempToken;
    psContext->eTokenType = eTempTokenType;
}



/***************************************************************************
Function    :  get_next_UserFunction
Description :  Return index of next free position in function array.
Returns     :  -1 is returned if the array is full.
               -2 is returned when duplicate function is found.
***************************************************************************/
int get_next_UserFunction(char *s)
{
    int t;

    for(t=0;t<NUM_FUNC;++t)
    {
        if(psContext->sUserFunctionTable[t].name[0]==0) return t;
        if(!strcmp(psContext->sUserFunctionTable[t].name,s)) return -2; /* duplicate ! */
    }

  return -1;
}

/***************************************************************************
Function    :  find_UserFunction
Description :  Find location of given function.
Returns     :  Null if function is not found;
               otherwise a pointer to the position of the function is returned.
***************************************************************************/
char *find_UserFunction(char *s)
{
    int t;
    
    for(t=0; t<NUM_FUNC; ++t)
    {       
        if(!strcmp(psContext->sUserFunctionTable[t].name,s))
        {
            return psContext->sUserFunctionTable[t].p;
        }
    }

  return '\0'; /* error condition */
}


/***************************************************************************
Function    :  userFunction_init
Description :  Initialize the array that holds the functions.
               By convention, a null function name indicates that array
               position is unused.
Returns     :  Nothing
***************************************************************************/
void userFunction_init(void)
{
    int t;

    for(t=0; t<NUM_FUNC; t++) psContext->sUserFunctionTable[t].name[0]='\0';
}



/***************************************************************************
Function    :  basic_CreateContext
Description :  Allocates memory on the context stack for a new instance of
               the BASIC interpretor.
Returns     :  SUCCESS = index of the new context on the context stack
               FAIL = -1
***************************************************************************/
int basic_CreateContext(char *pcFileName, char *pcArguments)
{
    int x;
    int iStatus = -1;
    tsBasicContext *psNewContext = NULL;
    tsBasicContext *psParentContext = NULL;
    char *pcNewProgram = NULL;
    int iIndex;
    char *pcTemp;
    int iTemp;
    int iArgument;
    char acArgName[255];
    char acArguments[1500];
    double fTemp;

    /*Create a new context and push onto the context stack*/
    if (iContextIndex < (BASIC_RECURSION_DEPTH-1))
    {
        /*Allocate memeory for the context*/
        psNewContext = malloc(sizeof(tsBasicContext));

        if (psNewContext != NULL)
        {
            /*Allocate memory for the program */
	        pcNewProgram = malloc(PROG_SIZE);

            if (pcNewProgram != NULL)
            {
                /*Push new context on context stack*/
                apsContextStack[++iContextIndex] = psNewContext;
                psContext = psNewContext;
                iStatus = iContextIndex;
                
                /*Link context to program and set the Program Counter*/
                psContext->pcProgram = pcNewProgram;
                psContext->pcProgramCounter = pcNewProgram;
                //strcpy(psContext->acFileName, pcFileName);
                STRNCPY(psContext->acFileName, pcFileName, sizeof(psContext->acFileName));
            }
            else
	        {
		        basic_printf("BASIC: memory allocation failure while loading program\n");
                
                /*No memory for program so delete the context*/
                free(psNewContext);
		        iStatus = -1;
	        }            
        }
        else
        {
            basic_printf("BASIC: memory allocation failure while creating context\n");
            iStatus = -1;
        }
    }

    /*Initialise the new context*/
    if (iStatus != -1)
    {
        label_init();
        userFunction_init();

        /*Clear all file handles*/
        for(x=0; x<11; x++)
        {
            psContext->apFileHandles[x] = NULL;
        }
        
        /*Clear all integer variables*/
        for(x=0; x < NUM_INTEGER_VARIABLES; x++)
        {
            strcpy(psContext->asIntegerVariables[x].acName, "UNUSED INTEGER VARIABLE");
            psContext->asIntegerVariables[x].iValue = 0;
        }

        /*Clear all float variables*/
        for(x=0; x < NUM_FLOAT_VARIABLES; x++)
        {
            strcpy(psContext->asFloatVariables[x].acName, "UNUSED FLOAT VARIABLE");
            psContext->asFloatVariables[x].fValue = 0;
        }

        /*Clear all string variables*/
        for(x=0; x < NUM_STRING_VARIABLES; x++)
        {
            strcpy(psContext->asStringVariables[x].acName, "UNUSED STRING VARIABLE");
            psContext->asStringVariables[x].acValue[0] = 0;
        }

        /*Clear all array variables*/
        for(x=0; x < NUM_ARRAY_VARIABLES; x++)
        {
            strcpy(psContext->asArrayVariables[x].acName, "UNUSED ARRAY VARIABLE");
            psContext->asArrayVariables[x].pcValue = NULL;
        }

        /*Clear all shared variable names*/
        for(x=0; x < NUM_SHARED_VARIABLES; x++)
        {
            strcpy(psContext->acSharedVariables[x], "UNUSED SHARED VARIABLE");
        }
        
        /*Clear all common variable names*/
        for(x=0; x < NUM_COMMON_VARIABLES; x++)
        {
            strcpy(psContext->acCommonVariables[x], "UNUSED COMMON VARIABLE");
        }

        /*Initialise RESERVED variables*/ 
        update_SnmpVariables();
        update_DateAndTime();

        // /*Store IP address assigned to SNMP tool in the variable Address$*/
	    // iIndex = find_Variable("address$", STRINGVARIABLE);
        // if (iIndex == -1) iIndex = create_Variable("address$", STRINGVARIABLE);
        // strcpy(psContext->asStringVariables[iIndex].acValue, acSnmpIp);

        // /*Store Target IP address in the variable Target$*/
        // iIndex = find_Variable("target$", STRINGVARIABLE);
        // if (iIndex == -1) iIndex = create_Variable("target$", STRINGVARIABLE);
        // strcpy(psContext->asStringVariables[iIndex].acValue, acIpAddress);
        
        // /*Store Target Context ID in the variable ContextId$*/
        // iIndex = find_Variable("contextid$", STRINGVARIABLE);
        // if (iIndex == -1) iIndex = create_Variable("contextid$", STRINGVARIABLE);
        // strcpy(psContext->asStringVariables[iIndex].acValue, acContextId);
        
        /*Store SNMP DEBUGGER version label in Version$*/
        iIndex = find_Variable("version$", STRINGVARIABLE);
        if (iIndex == -1) iIndex = create_Variable("version$", STRINGVARIABLE);
        strcpy(psContext->asStringVariables[iIndex].acValue, "00.00.00"/*SNMP_VERSION*/);
        
        /*Create blank INKEY$ to hold the last keystroke*/
        iIndex = find_Variable("inkey$", STRINGVARIABLE);
        if (iIndex == -1) iIndex = create_Variable("inkey$", STRINGVARIABLE);
        strcpy(psContext->asStringVariables[iIndex].acValue, "");

        /*Create ReturnValue$*/
	    iIndex = find_Variable("returnvalue$", STRINGVARIABLE);
        if (iIndex == -1) iIndex = create_Variable("returnvalue$", STRINGVARIABLE);
        strcpy(psContext->asStringVariables[iIndex].acValue, "");

        /*Create ReturnValue*/
	    iIndex = find_Variable("returnvalue", FLOATVARIABLE);
        if (iIndex == -1) iIndex = create_Variable("returnvalue", FLOATVARIABLE);

        /*Create ReturnValue%*/
	    iIndex = find_Variable("returnvalue%", INTEGERVARIABLE);
        if (iIndex == -1) iIndex = create_Variable("returnvalue%", INTEGERVARIABLE);


        /*Copy parent context's COMMON variables*/
        if (iContextIndex > 0)
        {            
            psParentContext = apsContextStack[iContextIndex-1];
            for(x=0; x < NUM_COMMON_VARIABLES; x++)
            {
                if (strcmp(psParentContext->acCommonVariables[x], "UNUSED COMMON VARIABLE"))
                {
                    if (psParentContext->acCommonVariables[x][strlen(psParentContext->acCommonVariables[x])-1] == '$')
                    {                                                                   
                        /*switch to parent context to read string variable value*/
                        psContext = psParentContext;

                        /*Find the Common variable*/
                        iIndex = find_Variable(psContext->acCommonVariables[x], STRINGVARIABLE);
                                
                        if (iIndex >= 0)
                        {
                            /*Variable found OK, so copy it*/                       
                            pcTemp = psContext->asStringVariables[iIndex].acValue;

                            /*Switch to new context to store variable value*/
                            psContext = psNewContext;

                            /*Create new variable in child context*/
                            iIndex = create_Variable(psParentContext->acCommonVariables[x], STRINGVARIABLE);
                            strcpy(psContext->asStringVariables[iIndex].acValue, pcTemp);
                        }
                               
                        /*Unconditionally switch to new context*/
                        psContext = psNewContext;                       

                    }
                    else if (psParentContext->acCommonVariables[x][strlen(psParentContext->acCommonVariables[x])-1] == '%')
                    {
                        /*switch to parent context to read integer variable value*/
                        psContext = psParentContext;

                       /*Find the Common Variable*/
                        iIndex = find_Variable(psContext->acCommonVariables[x], INTEGERVARIABLE);

                        if (iIndex >= 0)
                        {
                            iTemp = psContext->asIntegerVariables[iIndex].iValue;                                                                                                          

                            /*switch to new context to store variable value*/
                            psContext = psNewContext;

                            /*integer variable*/
                            iIndex = find_Variable(psParentContext->acCommonVariables[x], INTEGERVARIABLE);
                            if (iIndex == -1) iIndex = create_Variable(psParentContext->acCommonVariables[x], INTEGERVARIABLE);
 
                            psContext->asIntegerVariables[iIndex].iValue = iTemp;
                        }

                        /*Unconditionally switch to new context*/
                        psContext = psNewContext;   
                    } 
                    else
                    {
                        /*switch to parent context to read float variable value*/
                        psContext = psParentContext;

                       /*Find the Common Variable*/
                        iIndex = find_Variable(psContext->acCommonVariables[x], FLOATVARIABLE);

                        if (iIndex >= 0)
                        {
                            fTemp = psContext->asFloatVariables[iIndex].fValue;                                                                                                          

                            /*switch to new context to store variable value*/
                            psContext = psNewContext;

                            /*integer variable*/
                            iIndex = find_Variable(psParentContext->acCommonVariables[x], FLOATVARIABLE);
                            if (iIndex == -1) iIndex = create_Variable(psParentContext->acCommonVariables[x], FLOATVARIABLE);
 
                            psContext->asFloatVariables[iIndex].fValue = fTemp;
                        }

                        /*Unconditionally switch to new context*/
                        psContext = psNewContext;   
                    }                    
                }
            }
        }

        psContext->iThenElseLine = 0;


        /*Copy arguments passed on command line.  These have priority over "common" arguments.*/
        if(pcArguments)
        {
            /*Make local copy of arguments (we are going to use strtok, which is destructive)*/
            strcpy(acArguments, pcArguments);
      
            iArgument = 1;
            pcTemp = strtok(acArguments, " ");
            while(pcTemp)
            {
                /*Create argument variable name*/
                sprintf(acArgName, "arg%d$", iArgument++);

                iIndex = find_Variable(acArgName, STRINGVARIABLE);
                if (iIndex < 0)
                {
                    /*Create new string variable*/
                    iIndex = create_Variable(acArgName, STRINGVARIABLE);                    
                } 
                
                strcpy(psContext->asStringVariables[iIndex].acValue, pcTemp);

                /*Find next argument*/
                pcTemp = strtok(NULL, " ");
                                
            }            
        }        
        
        /*Create blank arguments if not created already*/
        for(iArgument=1; iArgument < 6; iArgument++)
        {

            /*Create argument variable name*/
            sprintf(acArgName, "arg%d$", iArgument);

            iIndex = find_Variable(acArgName, STRINGVARIABLE);
            if (iIndex < 0)
            {
                /*Create new string variable containing a NULL string*/
                iIndex = create_Variable(acArgName, STRINGVARIABLE);
                strcpy(psContext->asStringVariables[iIndex].acValue, "");
            }
        }

    }
    
    return(iStatus);

} /*end basic_CreateContext*/


/***************************************************************************
Function    :  basic_DestroyContext
Description :  Deletes memory on the context stack for a current context.
               (the current context always refers to the last instance of the
               BASIC interpretor that was spawned).
Returns     :  Index of new context OR -1 if no instances of the interpreter
               remain
***************************************************************************/
int basic_DestroyContext(void)
{
    int x;
    int iStatus = 0;
    tsBasicContext *psChildContext = NULL;
    tsBasicContext *psParentContext = NULL;
    char *pcTemp;
    int iTemp;
    int iIndex;
    int iCurrentContextIndex;
    double fTemp;

    // clear interactive shell context
    context_initialized = false;

    if (iContextIndex != -1)
    {
        
        /*Remember the index of the Context we are destroying*/
        iCurrentContextIndex = iContextIndex;

        /*Destroy the current context and all contexts above it on the stack*/
        for(iContextIndex = BASIC_RECURSION_DEPTH-1; iContextIndex >= iCurrentContextIndex; iContextIndex--)
        {
            /*If context exists then destroy it*/
            if (apsContextStack[iContextIndex] != NULL)
            {

                /*Switch to context to be destroyed*/
                psContext = apsContextStack[iContextIndex];

                /*Close any files left open by evil script writers*/
                for (x = 0; x <= 10; x++)
                {        
                    if (psContext->apFileHandles[x] != NULL)
                    {
                        fclose(psContext->apFileHandles[x]);
                        psContext->apFileHandles[x] = NULL;
                    }
                }               
        

                /*Copy childs's SHARED variables to parent*/
                if (iContextIndex > 0)
                {            
                    psChildContext = apsContextStack[iContextIndex];
                    psParentContext = apsContextStack[iContextIndex-1];
                    for(x=0; x < NUM_SHARED_VARIABLES; x++)
                    {
                        if (strcmp(psContext->acSharedVariables[x], "UNUSED SHARED VARIABLE"))
                        {
                            if (psContext->acSharedVariables[x][strlen(psContext->acSharedVariables[x])-1] == '$')
                            {                        
                                /*Find the shared variable*/
                                iIndex = find_Variable(psContext->acSharedVariables[x], STRINGVARIABLE);

                                
                                if (iIndex >= 0)
                                {
                                    /*Variable found OK, so copy it*/
                                    pcTemp = psContext->asStringVariables[iIndex].acValue;

                       
                                    /*switch to parent context to create variable value*/
                                    psContext = psParentContext;

                                    /*string variable*/
                                    iIndex = find_Variable(psChildContext->acSharedVariables[x], STRINGVARIABLE);
                                    if (iIndex == -1) iIndex = create_Variable(psChildContext->acSharedVariables[x], STRINGVARIABLE);

                                    strcpy(psContext->asStringVariables[iIndex].acValue, pcTemp);
                        
                                    /*switch to parent context to create variable value*/
                                    psContext = psChildContext;

                                }
                                else
                                {
                                    basic_printf("\nWarning: Shared variable did not exist on exit %s\n", psChildContext->acSharedVariables[x]);
        
                                }
                                              
                            }
                            else if (psContext->acSharedVariables[x][strlen(psContext->acSharedVariables[x])-1] == '%')
                            {                                                                                          
                                /*Find the Shared Variable*/
                                iIndex = find_Variable(psContext->acSharedVariables[x], INTEGERVARIABLE);

                                if (iIndex >= 0)
                                {
                                    iTemp = psContext->asIntegerVariables[iIndex].iValue;
                                    
                                    /*switch to parent context to read variable value*/
                                    psContext = psParentContext;
                        
                                    /*integer variable*/
                                    iIndex = find_Variable(psChildContext->acSharedVariables[x], INTEGERVARIABLE);
                                    if (iIndex == -1) iIndex = create_Variable(psChildContext->acSharedVariables[x], INTEGERVARIABLE);
 
                                    psContext->asIntegerVariables[iIndex].iValue = iTemp;
                       
                                    /*switch to new context to store variable value*/
                                    psContext = psChildContext;
                                }
                                else
                                {
                                    basic_printf("\nWarning: Shared variable did not exist on exit %s\n", psChildContext->acSharedVariables[x]);
                                }
                            }
                            else 
                            {                                                                                          
                                /*Find the Shared Variable*/
                                iIndex = find_Variable(psContext->acSharedVariables[x], FLOATVARIABLE);

                                if (iIndex >= 0)
                                {
                                    fTemp = psContext->asFloatVariables[iIndex].fValue;
                                    
                                    /*switch to parent context to read variable value*/
                                    psContext = psParentContext;
                        
                                    /*integer variable*/
                                    iIndex = find_Variable(psChildContext->acSharedVariables[x], FLOATVARIABLE);
                                    if (iIndex == -1) iIndex = create_Variable(psChildContext->acSharedVariables[x], FLOATVARIABLE);
 
                                    psContext->asFloatVariables[iIndex].fValue = fTemp;
                       
                                    /*switch to new context to store variable value*/
                                    psContext = psChildContext;
                                }
                                else
                                {
                                    basic_printf("\nWarning: Shared variable did not exist on exit %s\n", psChildContext->acSharedVariables[x]);
        
                                }
                            }
                        }    
                    }                
                }

                // copy return values from child to parent
                if (iContextIndex > 0)
                {            
                    psChildContext = apsContextStack[iContextIndex];
                    psParentContext = apsContextStack[iContextIndex-1];

                    /*Find the string return variable*/
                    iIndex = find_Variable("returnvalue$", STRINGVARIABLE);
                    
                    
                    if (iIndex >= 0)
                    {
                        /*Variable found OK, so copy it*/
                        pcTemp = psContext->asStringVariables[iIndex].acValue;
                        
                        
                        /*switch to parent context to create variable value*/
                        psContext = psParentContext;
                        
                        /*string variable*/
                        iIndex = find_Variable("returnvalue$", STRINGVARIABLE);
                        if (iIndex == -1) iIndex = create_Variable("returnvalue$", STRINGVARIABLE);
                        
                        strcpy(psContext->asStringVariables[iIndex].acValue, pcTemp);
                        
                        /*switch to parent context to create variable value*/
                        psContext = psChildContext;
                        
                    }
                    else
                    {
                        basic_printf("\nInternal Error: returnvalue$ not exist on exit\n");
                        
                    }
                    
                                                                                         
                    /*Find the integer return variable*/
                    iIndex = find_Variable("returnvalue%", INTEGERVARIABLE);
                    
                    if (iIndex >= 0)
                    {
                        iTemp = psContext->asIntegerVariables[iIndex].iValue;
                        
                        /*switch to parent context to read variable value*/
                        psContext = psParentContext;
                        
                        /*integer variable*/
                        iIndex = find_Variable("returnvalue%", INTEGERVARIABLE);
                        if (iIndex == -1) iIndex = create_Variable("returnvalue%", INTEGERVARIABLE);
                        
                        psContext->asIntegerVariables[iIndex].iValue = iTemp;
                        
                        /*switch to new context to store variable value*/
                        psContext = psChildContext;
                    }
                    else
                    {
                       basic_printf("\nInternal Error: returnvalue% not exist on exit\n");
                    }
                                                                                        
                    /*Find the float return variable*/
                    iIndex = find_Variable("returnvalue", FLOATVARIABLE);
                    
                    if (iIndex >= 0)
                    {
                        fTemp = psContext->asFloatVariables[iIndex].fValue;
                        
                        /*switch to parent context to read variable value*/
                        psContext = psParentContext;
                        
                        /*integer variable*/
                        iIndex = find_Variable("returnvalue", FLOATVARIABLE);
                        if (iIndex == -1) iIndex = create_Variable("returnvalue", FLOATVARIABLE);
                        
                        psContext->asFloatVariables[iIndex].fValue = fTemp;
                        
                        /*switch to new context to store variable value*/
                        psContext = psChildContext;
                    }
                    else
                    {
                        basic_printf("\nInternal Error: returnvalue not exist on exit\n");
                        
                    }                
                }

                /*Free Array variable memory*/
                for(x=0; x < NUM_ARRAY_VARIABLES; x++)
                {
                    if (psContext->asArrayVariables[x].pcValue)
                    {
                        //basic_printf("Free array %s\n", psContext->asArrayVariables[x].acName); 
                        free(psContext->asArrayVariables[x].pcValue);
                    }
                }

                /*Free the program and context memory*/
                if ((apsContextStack[iContextIndex] != NULL) &&
                    (apsContextStack[iContextIndex]->pcProgram != NULL))
                {
                    free(apsContextStack[iContextIndex]->pcProgram);
                    free(apsContextStack[iContextIndex]);
                }
            
                /*Mark context as unused*/
                apsContextStack[iContextIndex] = NULL;           
            
            }/*end if Context exists*/
                                

        } /*end for*/

        
        /*Switch to new context*/
        if (iContextIndex >=0 )
        {
            psContext = apsContextStack[iContextIndex];
        }
        else
        {
            psContext = NULL;
        }

    } /*end if context != -1*/

    return (iContextIndex);
}



/***************************************************************************
Function    :  create_Variable
Description :  Creates the variable.  Does NOT check for duplicates.
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
int create_Variable(char *pcName, teTokenType eTokenType)
{
    int iIndex;

    switch(eTokenType)
    {
    
    case STRINGVARIABLE:
        iIndex = find_Variable("UNUSED STRING VARIABLE", eTokenType);

        if (iIndex >= 0)
        {
            strncpy(psContext->asStringVariables[iIndex].acName, pcName, VAR_NAME_LEN);
        }
        else
        {
            basic_printf("Out of variable memory allocating string %s\n", pcName);
            syntax_error(SYNTAX);
        }
        break;

    case FLOATVARIABLE:
        iIndex = find_Variable("UNUSED FLOAT VARIABLE", eTokenType);

        if (iIndex >= 0)
        {
            strncpy(psContext->asFloatVariables[iIndex].acName, pcName, VAR_NAME_LEN);
        }
        else
        {
            basic_printf("Out of variable memory allocating float %s\n", pcName);
            syntax_error(SYNTAX);
        }
        break;

    case INTEGERVARIABLE:
        iIndex = find_Variable("UNUSED INTEGER VARIABLE", eTokenType);

        if (iIndex >= 0)
        {
            strncpy(psContext->asIntegerVariables[iIndex].acName, pcName, VAR_NAME_LEN);
        }
        else
        {
            basic_printf("Out of variable memory allocating integer %s\n", pcName);
            syntax_error(SYNTAX);
        }
        break;

    default:
        basic_printf("Major screw up.  Attempted to create a variable with a non-variable token\n");
        syntax_error(SYNTAX);
        break;

    } /*end switch*/

    return(iIndex);
}


/***************************************************************************
Function    :  find_Variable
Description :  Finds the index of the variable
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
int find_Variable(char *pcName, teTokenType eTokenType)
{
    int iIndex;

    switch(eTokenType)
    {
    
    case STRINGVARIABLE:
        for (iIndex=0; iIndex < NUM_STRING_VARIABLES; iIndex++)
        {
            /*Compare names.  Case sensitive.*/
            if (strcmp(psContext->asStringVariables[iIndex].acName, pcName) == 0)
            {
                /*Found*/
                break;
            }
        }

        /*Check if we found the variable*/
        if (iIndex >= NUM_STRING_VARIABLES)
        {
            /*Not Found*/
            iIndex = -1;
        }
        break;

    case FLOATVARIABLE:
        for (iIndex=0; iIndex < NUM_FLOAT_VARIABLES; iIndex++)
        {
            /*Compare names.  Case sensitive.*/
            if (strcmp(psContext->asFloatVariables[iIndex].acName, pcName) == 0)
            {
                /*Found*/
                break;
            }
        }

        /*Check if we found the variable*/
        if (iIndex >= NUM_FLOAT_VARIABLES)
        {
            /*Not Found*/
            iIndex = -1;
        }
        break;

    case INTEGERVARIABLE:
        for (iIndex=0; iIndex < NUM_INTEGER_VARIABLES; iIndex++)
        {
            /*Compare names.  Case sensitive.*/
            if (strcmp(psContext->asIntegerVariables[iIndex].acName, pcName) == 0)
            {
                /*Found*/
                break;
            }
        }

        /*Check if we found the variable*/
        if (iIndex >= NUM_INTEGER_VARIABLES)
        {
            /*Not Found*/
            iIndex = -1;
        }
        break;

    default:
        basic_printf("Screw up.  Attempted to find a variable with a non-variable token\n");
        syntax_error(SYNTAX);
        break;

    } /*end switch*/

    return(iIndex);
}


/***************************************************************************
Function    :  find_ArrayVariable
Description :  Finds the address of a variable within an array
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
char *find_ArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM])
{
    int iIndex = 0;
    int iOffset = 0;
    int iMultiplier = 0;
    int iArrayIndex = 0;
    char *pcValue = NULL;

    for(iIndex=0; iIndex < NUM_ARRAY_VARIABLES; iIndex++)
    {
        if (strcmp(pcName, psContext->asArrayVariables[iIndex].acName) == 0)
        {
            break;
        }
    }

    if (iIndex < NUM_ARRAY_VARIABLES)
    {
        for(iArrayIndex = 0; iArrayIndex < MAX_ARRAY_DIM; iArrayIndex++)
        {
            iOffset += psContext->asArrayVariables[iIndex].iMultiplier[iArrayIndex] * aiIndex[iArrayIndex];
        }

        pcValue = psContext->asArrayVariables[iIndex].pcValue + iOffset;
    }
    else
    {
        // check if array declared in callers context
        if (iContextIndex > 0)
        {
            // change to callers context
            iContextIndex--;
            psContext = apsContextStack[iContextIndex];

            pcValue = find_ArrayVariable(pcName, aiIndex);
            
            // change back to local context
            iContextIndex++;
            psContext = apsContextStack[iContextIndex];
        }
    }

    return(pcValue);
}



/***************************************************************************
Function    :  dump_Variables
Description :  Creates a dump of all variables
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
void dump_Variables(void)
{
    int iIndex;

    basic_printf("\n");
    basic_printf("**********************************************************\n");
    basic_printf("String Variables:\n");
    for (iIndex=0; iIndex < NUM_STRING_VARIABLES; iIndex++)
    {
        if (strcmp(psContext->asStringVariables[iIndex].acName, "UNUSED STRING VARIABLE"))
        {
            basic_printf("                                            %s\r%40s\n",
                   psContext->asStringVariables[iIndex].acValue,
                   psContext->asStringVariables[iIndex].acName);             
        }
    }

    basic_printf("Float Variables:\n");
    for (iIndex=0; iIndex < NUM_FLOAT_VARIABLES; iIndex++)
    {
        if (strcmp(psContext->asFloatVariables[iIndex].acName, "UNUSED FLOAT VARIABLE"))
        {
            basic_printf("                                            %G\r%40s\n",
                   psContext->asFloatVariables[iIndex].fValue,
                   psContext->asFloatVariables[iIndex].acName);             
        }

    }

    basic_printf("Integer Variables:\n");
    for (iIndex=0; iIndex < NUM_INTEGER_VARIABLES; iIndex++)
    {
        if (strcmp(psContext->asIntegerVariables[iIndex].acName, "UNUSED INTEGER VARIABLE"))
        {
            basic_printf("                                            %d\r%40s\n",
                   psContext->asIntegerVariables[iIndex].iValue,
                   psContext->asIntegerVariables[iIndex].acName);             
        }

    }
  
    if (strcmp(psContext->acCommonVariables[0], "UNUSED COMMON VARIABLE")) basic_printf("Common Variables:\n");
    for (iIndex=0; iIndex < NUM_COMMON_VARIABLES; iIndex++)
    {
        if (strcmp(psContext->acCommonVariables[iIndex], "UNUSED COMMON VARIABLE"))
        {
            basic_printf("%40s\n", psContext->acCommonVariables[iIndex]);             
        }

    }
  
    if (strcmp(psContext->acSharedVariables[0], "UNUSED SHARED VARIABLE")) basic_printf("Shared Variables:\n");
    for (iIndex=0; iIndex < NUM_SHARED_VARIABLES; iIndex++)
    {
        if (strcmp(psContext->acSharedVariables[iIndex], "UNUSED SHARED VARIABLE"))
        {
            basic_printf("%40s\n", psContext->acSharedVariables[iIndex]);             
        }

    }

    basic_printf("**********************************************************\n");


}


/***************************************************************************
Function    :  update_DateAndTime
Description :  Creates a dump of all variables
Returns     :  Index of variable OR -1 if not found
***************************************************************************/
void update_DateAndTime(void)
{
    int iIndex;

    /*Store Time in variable Time$*/
    iIndex = find_Variable("time$", STRINGVARIABLE);
    if (iIndex == -1) iIndex = create_Variable("time$", STRINGVARIABLE);
    get_local_time_string(psContext->asStringVariables[iIndex].acValue, sizeof(psContext->asStringVariables[iIndex].acValue));                

    /*Store Date in variable Date$*/
    iIndex = find_Variable("date$", STRINGVARIABLE);
    if (iIndex == -1) iIndex = create_Variable("date$", STRINGVARIABLE);
    get_local_date_string(psContext->asStringVariables[iIndex].acValue, sizeof(psContext->asStringVariables[iIndex].acValue));       

    // Store Day in variable Day$
    iIndex = find_Variable("day$", STRINGVARIABLE);
    if (iIndex == -1) iIndex = create_Variable("day$", STRINGVARIABLE);
    get_local_day_string(psContext->asStringVariables[iIndex].acValue, sizeof(psContext->asStringVariables[iIndex].acValue));      

}
 

/***************************************************************************
Function    :  display_ScriptLine
Description :  display current script file line
Returns     :  nothing
***************************************************************************/
void display_ScriptLine(void)
{
    char *p;
    char *temp;
    int linecount = 0;
    int i;

    p = psContext->pcProgram;
    while(p != psContext->pcProgramCounter)
    {
        /*find line number of error*/
        p++;
        //if(*p == '\r')  // original for DOS EOL
        if(*p == '\n')    // should work for both DOS and UNIX
        {
            linecount++;
        }
    }
    basic_printf("line %d\n", linecount);

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


    /*print out the lines*/
    for(; p<=temp; p++) basic_printf("%c", *p);
}


/***************************************************************************
Function    :  basic_Stop
Description :  Display BASIC program source file and Highlight next command
               to be executed.  Wait for user input before proceeding.
Returns     :  nothing
***************************************************************************/
void basic_Stop(void)
{
    int iKey;
    char acStopMessage[256];

    /*Enable Stepping*/
    bSteppingActive = 1;
    
    /*Ensure Trace Window is Open*/
    if (!bTraceActive)
    {
        bTraceActive = 1;
        //sock_TraceOpen(psContext->acFileName);
    }

    /*If we came in here on a stop command move PC back to highlight the "STOP"*/
    if (psContext->eToken == STOP)
    {
        putback();
    }

    // if (sock_TraceShow(psContext->pcProgramCounter - psContext->pcProgram) > 0)
    // {            
    //     sprintf(acStopMessage, "SCRIPT STOPPED - %s", psContext->acFileName); 
    //     SetConsoleTitle(acStopMessage);

    //     iKey = getch();           

    //     SetProgramTitle();

    //     if ((iKey == 0x00) || (iKey == 0xE0))
    //     {
	// 	    /*Handle Extended Keys*/
	// 		iKey = getch() + 256;
	// 	}
        
    //     switch(iKey)
    //     {
    //         case '?':
    //         case F1:
    //             basic_printf("\n");
    //             basic_printf("BASIC DEBUG HELP     \n");
    //             basic_printf("================     \n");
    //             basic_printf("F5   Run             \n");
    //             basic_printf("F10  Step            \n");
    //             basic_printf("D    Dump Variables  \n");
    //             basic_printf("ESC  Terminate Script\n");
    //             break;
                
    //         case 'd':
    //         case 'D':
    //             dump_Variables();
    //             break;

    //         case 'r':
    //         case 'R':
	// 		case F5:
    //             bSteppingActive = 0;
    //             sock_TraceHide();
    //             //bring this window to the front ?
    //             break;

	// 		case F10:
    //             break;

    //         case ESC:
    //  			/*Terminate request*/
	// 		    bScriptFileActive = 0;
    //             bSteppingActive = 0;
    //             bTerminateWithExtremePrejudice = 1; // terminate parent scripts 
    //             break;
    //     }
    // }
    
    /*If we came in here on a stop command move PC past it*/
    if (psContext->eToken == STOP)
    {
        get_token();
    }

             
}


/***************************************************************************
Function    :  basic_Trace
Description :  Run a script with stepping enabled
Returns     :  nothing
***************************************************************************/
void basic_Trace(void)
{
    /*Enable Stepping*/
    bSteppingActive = 1;

    basic_Run();
}


/***************************************************************************
Function    :  basic_InterpretFunction
Description :  Launchs a new instance of the BASIC interpreter and executes
               the function named.
Returns     :  0 - OK
               1 - error initiaising BASIC interpreter
***************************************************************************/
int basic_InterpretFunction(char *pcFunctionBody, int iFuncNum)
{
	int x;
	int iKey;
    int iInkeyIndex;
    int iParamNum;
    int iIndex;

    /*Create a new BASIC Context*/
    basic_CreateContext("FUNCTION CALL", NULL);


    // copy some info from callers context
    memcpy(psContext->pcProgram, apsContextStack[iContextIndex-1]->pcProgram, PROG_SIZE);
    psContext->pcProgramCounter = psContext->pcProgram + (pcFunctionBody - apsContextStack[iContextIndex-1]->pcProgram); 
    strcpy(psContext->acFileName, apsContextStack[iContextIndex-1]->acFileName);

    // copy parameters from parent context
    for (iParamNum=0; iParamNum < 10; iParamNum++)
    {
        switch(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].eParamType)
        {
        case INTEGERVARIABLE:                       
            iIndex = find_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamInt.acName, INTEGERVARIABLE);
            if (iIndex == -1) iIndex = create_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamInt.acName, INTEGERVARIABLE);
            psContext->asIntegerVariables[iIndex].iValue = apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamInt.iValue;            
            break;
            
        case FLOATVARIABLE:           
            iIndex = find_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamFlt.acName, FLOATVARIABLE);
            if (iIndex == -1) iIndex = create_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamFlt.acName, FLOATVARIABLE);
            psContext->asFloatVariables[iIndex].fValue = apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamFlt.fValue;            
            break;
            
        case STRINGVARIABLE:            
            iIndex = find_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamStr.acName, STRINGVARIABLE);
            if (iIndex == -1) iIndex = create_Variable(apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamStr.acName, STRINGVARIABLE);
            strcpy(psContext->asStringVariables[iIndex].acValue, apsContextStack[iContextIndex-1]->sUserFunctionTable[iFuncNum].asParameters[iParamNum].sParamStr.acValue);
            break;
            
        default:
            // parameter not used - do nothing
            break;
        }
    }

    /*Reload Trace Window*/
//    if (bTraceActive)
//    {
//        sock_TraceOpen(psContext->acFileName);
//    }

	/*The code in the if block only executes if an error occurs*/
    if(setjmp(psContext->sEnviroment))
    {
        
        /*Erase the current context*/
        basic_DestroyContext();

#ifdef PARENTS_CONTINUE_AFTER_ERROR
        //THIS CODE controls whether parent scripts are terminated
        //when a child script encounters an error
        
        /*Check for nested BASIC programs*/
        if (iContextIndex >=0)
        {
            /*Acitvate the parent program*/
            bScriptFileActive = 1;
        }
#endif
        
        return(1); 
    }

	scan_labels();                           /*find the labels in the program*/
    scan_UserFunctions();                    /*find the functions in the program*/
	psContext->iTopOfForStack = 0;           /*initialize the FOR stack index*/
	psContext->iTopOfWhileStack = 0;         /*initialize the WHILE stack index*/
	psContext->iTopOfGosubStack = 0;         /*initialize the GOSUB stack index*/


    /*Remember index of INKEY$ variable to quickly store last keystroke*/
    iInkeyIndex = find_Variable("inkey$", STRINGVARIABLE);
    if (iInkeyIndex == -1) iInkeyIndex = create_Variable("inkey$", STRINGVARIABLE);


	/*Execute Script*/
	bScriptFileActive = 1;
	do
	{
		//x = 0;

        /*Get the next token*/
        psContext->eTokenType = get_token();

        //basic_printf("Token String = %s Token = %d Type = %d\n", psContext->acToken, psContext->eToken, psContext->eTokenType); 


		/*Check for assignment statement*/
		if((psContext->eTokenType==INTEGERVARIABLE) ||
		   (psContext->eTokenType==FLOATVARIABLE) ||
           (psContext->eTokenType==STRINGVARIABLE))
		{
			/*This is a LET statement without the optional LET keyword*/
            putback();
            if (bSteppingActive) basic_Stop();
			basic_Let();        
		}
        else if (psContext->eTokenType==USERFUNCTION)
        {
            /*Trace the function call*/
            if (bSteppingActive)
            {
                putback();
                basic_Stop();
                get_token();
            }

            basic_Function();

        }
		else
		{
            /*Assume it is a command*/
			for(x= 0; x < sizeof(asCommandTable)/sizeof(asCommandTable[0]); x++)
            {
                if (psContext->eToken == asCommandTable[x].eToken)
                {
                    /*Trace the command*/
                    if ((bSteppingActive) &&
                        (psContext->eToken != REM) &&
                        (psContext->eToken != STOP))
                    {
                        putback();
                        basic_Stop();
                        get_token();
                    }

                    /*Perform the command*/
                    asCommandTable[x].pfCommand();
                    break;
                }
            }            

		} /*end else*/


		/*Check if user has pressed ESCAPE*/
		// if (kbhit())
		// {
		// 	iKey = getch();
		// 	if ((iKey == 0x00) || (iKey == 0xE0))
		// 	{
		// 		/*Handle Extended Keys*/
		// 		iKey = getch() + 256;
		// 	}

		// 	if (iKey == ESC)
		// 	{                
		// 		/*Terminate request*/
		// 		bScriptFileActive = 0;

        //         /*Force termination of all scripts*/
        //         bTerminateWithExtremePrejudice = 1;        

		// 	}

        //     /*Update INKEY$ value for BASIC scripts to read*/
        //     sprintf(psContext->asStringVariables[iInkeyIndex].acValue, "%c",iKey);
		// }
        // else if (bClearInkey == 1)
        // {
        //     /*Clear INKEY$ since it has been read by the BASIC script*/
        //     sprintf(psContext->asStringVariables[iInkeyIndex].acValue, "");
        //     bClearInkey = 0;
        // }


		/*Check for terminate request. END, INPUT or ESCAPE*/
		if (!bScriptFileActive) break;

       

	} while (psContext->eToken != FINISHED);

    /*Erase the current context*/
    basic_DestroyContext();

    /*Check for nested BASIC programs*/
    if (iContextIndex >=0)
    {
        /*If user pressed ESCAPE do not reactivate parent program*/
        if (!bTerminateWithExtremePrejudice)
        {
            /*Reactivate the parent program*/            
            bScriptFileActive = 1;
        }

        /*Reload Trace Window*/
//        if (bTraceActive)
//        {
//            sock_TraceOpen(psContext->acFileName);
//        }
    }
    else
    {
        /*Disable Stepping*/
        bSteppingActive = 0;
        
        if (bTraceActive)
        {
            /*Close the trace Window*/
            //sock_TraceHide();
            bTraceActive = 0;
        }

        /*Disable forced termination of all scripts*/
        bTerminateWithExtremePrejudice = 0;        
    }

	return 0;
}
    
