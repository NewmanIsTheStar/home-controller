/******************************************************************************
File:		basic_extensions
Description:This file contains all of the non-standard commands added to the
            BASIC language in order to support SNMP, auotmated testing
            and various other functions.
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
//#include "net.h"
#include "lwip/sockets.h"
#include "shelly.h"
#include "utility.h"

#define COMMAND_LINE_LENGTH 1500
#define noSuchObject	0x80
#define noSuchInstance	0x81

/*External Variables*/
extern tsBasicContext *psContext;
// extern char acLastValue[];                  //last SNMP value received
// extern int iErrStat;                        //error status of last SNMP command
// extern char acLastOid[];					//last OID received (includes instance)
// extern char acLastOctetString[];			//last OCTET STRING received
// extern int LastOctetStringLength;			//length of the last OCTET STRING received
// extern int bQuiet;                          //Quiet Mode
// extern char acSnmpIp[];                     //SNMP's own IP address
// extern char acContextId[20];                //Target Agent's context ID
// extern char acIpAddress[];                  //Target Agent's IP address
// extern char acReturnedInstance[200];        //last instance received
// extern char *pacLastName;                   //Pointer to last OID name received
// extern int bTimeStamp;                      //time stamp flag
// extern int iMaxRetries;						//user defined maximum number of retries
// extern int bTerminal;						//terminal emulation flag
// extern FILE *logfile;
// extern int iInterCharDelay;                 //intercharacter delay used for slow devices


/*Public Variable*/
char bSuperSlowMode = 0;

/*Private Variables*/


/*Local Prototype*/
void get_Parameter(char *pcCommandLine);
void basic_SetFocus(void);


int processCommand(char *pcString)
{
    printf("Basic Extension not implemented\n");

    return(0);
}


/***************************************************************************
Function    :  basic_Colour
Description :  Control text colour
Returns     :  Nothing
***************************************************************************/
void basic_Colour(void)
{
	// HANDLE hConsoleScreenBuffer = INVALID_HANDLE_VALUE;
    // WORD wColour;
    // int iTemp = 0;
  

    // get_Bracket('(');

    // /*Get Colour*/
    // get_token();

    // switch(psContext->eTokenType)
    // {

    // case STRINGVARIABLE:
    //     sscanf(get_StringVariable(psContext->acToken), "%d", &iTemp);
    //     break;

    // case NUMBER:
    //     sscanf(psContext->acToken, "%d", &iTemp);
    //     break;

    // default:
    //     putback();
    //     eval_IntegerExpression(&iTemp);
    //     break;

    // }

    // get_Bracket(')');

    // /*Get Console Ouput Buffer handle*/
    // hConsoleScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE);

    // if (hConsoleScreenBuffer != INVALID_HANDLE_VALUE)
    // {    
    //     /*Set the Text colour*/
    //     wColour = (WORD)iTemp;
    //     SetConsoleTextAttribute(hConsoleScreenBuffer, wColour);

    //     //CloseHandle(hConsoleScreenBuffer); // no don't do this !  
    // }

} /*end basic_Colour*/


/***************************************************************************
Function    :  basic_Get
Description :  SNMP GET
Returns     :  Nothing
***************************************************************************/
void basic_Get(void)
{
    int value = 0;
    char acTemp[COMMAND_LINE_LENGTH];

    /*Begin creating text string to pass to command interpreter*/
    strcpy(acTemp, "Get ");

    /*Get the opening bracket*/
    get_Bracket('(');


    /*Get the Name*/
    get_token();
	if(psContext->eTokenType==STRINGVARIABLE)
	{
        strcat(acTemp, get_StringVariable(psContext->acToken));
	}
	else
	{
		strcat(acTemp, psContext->acToken);
	}

    strcat(acTemp, " ");

    /*Get the comma*/
    get_token();

    /*Get the instance*/
    get_Parameter(acTemp);

    /*Get the closing bracket*/
    get_Bracket(')');

    /*Pass the command string to the interpreter for processing*/
    processCommand(acTemp);

    update_SnmpVariables();

} /*End basic_Get*/



/***************************************************************************
Function    :  basic_GetNext
Description :  SNMP GETNEXT
Returns     :  Nothing
***************************************************************************/
void basic_GetNext(void)
{
    int value = 0;
    char acTemp[COMMAND_LINE_LENGTH];

    strcpy(acTemp, "GetNext ");

    /*Get opening bracket*/
    get_Bracket('(');

    get_token();
    if (*psContext->acToken != ')')
    {
        if(psContext->eTokenType==STRINGVARIABLE)
        {
            strcat(acTemp, get_StringVariable(psContext->acToken));
	    }
	    else
	    {
		    strcat(acTemp, psContext->acToken);
	    }
        strcat(acTemp, " ");
	    get_token();
    }

    if (*psContext->acToken == ',')
    {
        /*Get instance*/
        get_Parameter(acTemp);
        get_token();
    }

    /*Get closing bracket*/
    if (*psContext->acToken != ')')
    {
	    syntax_error(UNBAL_PARENS);
    }

    /*Pass command string to interpreter*/
    processCommand(acTemp);

    update_SnmpVariables();

} /*End basic_GetNext*/



/***************************************************************************
Function    :  basic_Set
Description :  SNMP SET
Returns     :  Nothing
***************************************************************************/
void basic_Set(void)
{
    int value = 0;
    char acTemp[COMMAND_LINE_LENGTH];
    unsigned int iLength = 0;

    strcpy(acTemp, "Set ");

    /*Get opening bracket*/
    get_Bracket('(');

    /*Get name*/
    get_token();
	if(psContext->eTokenType==STRINGVARIABLE)
	{
        strcat(acTemp, get_StringVariable(psContext->acToken));
	}
	else
	{
		strcat(acTemp, psContext->acToken);
	}
    strcat(acTemp, " ");

    /*Get comma*/
    get_token();

    iLength = strlen(acTemp);

    /*Get instance.  The instance parameter may be NULL if it was appended to the name with a dot*/
    get_Parameter(acTemp);

    /*Only append a space if the instance was not NULL*/
    if (iLength != strlen(acTemp))
    {
        strcat(acTemp, " ");
    }

    /*comma*/
    get_token();

    /*value*/
    get_Parameter(acTemp);

    /*Get closing bracket*/
    get_Bracket(')');

    /*Pass command string to command interpretor*/
    processCommand(acTemp);

    update_SnmpVariables();

} /*end basic_Set*/



/***************************************************************************
Function    :  basic_Quiet
Description :
Returns     :  Nothing
***************************************************************************/
void basic_Quiet(void)
{

    // get_token();

    // if (strcmp(psContext->acToken, "on") == 0)
    // {
    //     bQuiet = 1;
    // }
    // else if (strcmp(psContext->acToken, "off") == 0)
    // {
    //     bQuiet = 0;
    // }
    // else
    // {
    //     /*Token was not a valid state so put it back*/
    //     putback();

    //     /*User has not supplied state, so toggle the current state*/
    //     if (bQuiet == 0)
	// 	{
	// 	    bQuiet = 1;
	// 	}
	// 	else
	// 	{   
	// 		bQuiet = 0;
	// 	}
    // }
}


/***************************************************************************
Function    :  basic_TimeStamp
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_TimeStamp(void)
{
	// /*Toggle Timestamps on/off*/

	// if (bTimeStamp == 0)
	// {
	// 	bTimeStamp = 1;
	// }
	// else
	// {
	// 	bTimeStamp = 0;
	// }

}


/***************************************************************************
Function    :  basic_Verbose
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Verbose(void)
{
  char acTemp[COMMAND_LINE_LENGTH];

  strcpy(acTemp, "Vebose");

  processCommand(acTemp);
}


/***************************************************************************
Function    :  basic_Tftp
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Tftp(void)
{
  char acTemp[COMMAND_LINE_LENGTH];

  strcpy(acTemp, "Tftp");

  processCommand(acTemp);
}


/***************************************************************************
Function    :  basic_Target
Description :  Sets the IP address, UDP port and community string required
               to access the "target" SNMP agent
Returns     :  Nothing
***************************************************************************/
void basic_Target(void)
{/*  ORIGINAL
	char acTemp[COMMAND_LINE_LENGTH];
    int iIndex;

	strcpy(acTemp, "Target ");

    get_Bracket('(');

	get_token(); //IP Address
	if(psContext->eTokenType==STRINGVARIABLE)
	{
		iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
        strcat(acTemp, psContext->asStringVariables[iIndex].acValue);
	}
	else
	{
		strcat(acTemp, psContext->acToken);
	}

    get_Bracket(')');

	processCommand(acTemp);
*/
	// char acTemp[COMMAND_LINE_LENGTH];
    // int iIndex;

	// strcpy(acTemp, "Target ");

    // get_Bracket('(');

    // get_Parameter(acTemp);

    // get_Bracket(')');

	// processCommand(acTemp);

	// /*Store IP address assigned to SNMP tool in the variable A$*/
	// iIndex = find_Variable("address$", STRINGVARIABLE);
    // strcpy(psContext->asStringVariables[iIndex].acValue, acSnmpIp);

    // /*Store basic_Target IP address in the variable T$*/
    // iIndex = find_Variable("target$", STRINGVARIABLE);
    // strcpy(psContext->asStringVariables[iIndex].acValue, acIpAddress);

} /*end Target*/



/***************************************************************************
Function    :  basic_ContextId
Description :  MID A$ B$ x y where A$ is source, B$ is destination, x and y
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_ContextId(void)
{
/* ORIGINAL
    char acTemp[COMMAND_LINE_LENGTH];
    int iIndex;

    strcpy(acTemp, "ContextId ");

    get_Bracket('(');

    //Community String
    get_token();
	if(psContext->eTokenType==STRINGVARIABLE)
	{
		iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
        strcat(acTemp, psContext->asStringVariables[iIndex].acValue);
		set_StringVariable("contextid$", psContext->asStringVariables[iIndex].acValue);
	}
	else
	{
		strcat(acTemp, psContext->acToken);
		set_StringVariable("contextid$$", psContext->acToken);
	}

    get_Bracket(')');

	processCommand(acTemp);

*/
	char acTemp[COMMAND_LINE_LENGTH];


	strcpy(acTemp, "ContextId ");

    get_Bracket('(');

    get_Parameter(acTemp);

    get_Bracket(')');

	processCommand(acTemp);



}


/***************************************************************************
Function    :  basic_LogToFile
Description :  MID A$ B$ x y where A$ is source, B$ is destination, x and y
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_LogToFile(void)
{
  int value = 0;
  int var = 0;
  char acTemp[COMMAND_LINE_LENGTH];
  int iIndex;

  strcpy(acTemp, "Log ");

    get_Bracket('(');

  get_token();
  if (*psContext->acToken != ')')
  {
	if(psContext->eTokenType==STRINGVARIABLE)
	{
		iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
        strcat(acTemp, psContext->asStringVariables[iIndex].acValue);
	}
	else
	{
		strcat(acTemp, psContext->acToken);
	}
    strcat(acTemp, " ");
	get_token();
  }

  if (*psContext->acToken == ',')
  {
    get_token(); /*file name*/
      if(psContext->eTokenType==STRINGVARIABLE)
  {
		iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
          strcat(acTemp, psContext->asStringVariables[iIndex].acValue);
  }
  else
    {
      /*literal instance*/
	  strcat(acTemp, psContext->acToken);
    }
	get_token();
  }

  if (*psContext->acToken != ')')
  {
	syntax_error(UNBAL_PARENS);
  }

  //printf("Command String: %s\n", acTemp);
  processCommand(acTemp);

} /*end basic_LogToFile*/



/***************************************************************************
Function    :  basic_Delay
Description :  Delay for specified number of deci-seconds               
Returns     :  Nothing
***************************************************************************/
void basic_Delay(void)
{
	char acTemp[COMMAND_LINE_LENGTH];


	strcpy(acTemp, "Delay ");

    get_Bracket('(');

	/*Delay in Deci Seconds*/
    get_Parameter(acTemp);

    get_Bracket(')');

	processCommand(acTemp);

}


/***************************************************************************
Function    :  basic_TimeOut
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_TimeOut(void)
{
	char acTemp[COMMAND_LINE_LENGTH];


	strcpy(acTemp, "Timeout ");

    get_Bracket('(');

	/*Delay in Milli Seconds*/
    get_Parameter(acTemp);

    get_Bracket(')');

	processCommand(acTemp);

} /*end basic_TimeOut*/


/***************************************************************************
Function    :  basic_Com
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Com(void)
{
	char acTemp[COMMAND_LINE_LENGTH];


	strcpy(acTemp, "COM ");

    get_Bracket('(');

	/*Delay in Milli Seconds*/
    get_Parameter(acTemp);

    get_Bracket(')');

    //printf("Executing command: %s\n", acTemp);
	processCommand(acTemp);

} /*end basic_Com*/


/***************************************************************************
Function    :  basic_RasAddr
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_RasAddr(void)
{
	// char acTemp[COMMAND_LINE_LENGTH];
    // int iIndex;


	// strcpy(acTemp, "RasAddr");

	// processCommand(acTemp);

	// /*Store IP address assigned to SNMP tool in the variable A$*/
	// iIndex = find_Variable("address$", STRINGVARIABLE);
    // strcpy(psContext->asStringVariables[iIndex].acValue, acSnmpIp);

    // /*Store basic_Target IP address in the variable T$*/
    // iIndex = find_Variable("target$", STRINGVARIABLE);
    // strcpy(psContext->asStringVariables[iIndex].acValue, acIpAddress);

} /*end basic_RasAddr*/


/***************************************************************************
Function    :  basic_Gpib
Description :  Get GPIB parameters here and pass them to the GPIB handler
Returns     :  Nothing
***************************************************************************/
void basic_Gpib(void)
{
    // int str_size =0;
    // char param[2][GPIB_STRING_LEN];
    // char *RtnParm = NULL;
    // int iIndex;
    // const int iNumArguments = 2;
    // int iArgNum;


    // get_Bracket('(');

    // for (iArgNum = 0; iArgNum < 2; iArgNum++)
    // {
    //     /*Get Argument*/
    //     get_token();

    //     if (psContext->eTokenType == QUOTE)
    //     {
    //         /* is string */
    //         strncpy(param[iArgNum], psContext->acToken, GPIB_STRING_LEN - 1);
            
    //     }
    //     else if (psContext->eTokenType == STRINGVARIABLE)
    //     {
    //         /* is string */
    //         iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    //         strncpy(param[iArgNum], psContext->asStringVariables[iIndex].acValue,
    //                 GPIB_STRING_LEN - 1);
            
    //     }
    //     else
    //     {
    //         /* is expression */
    //         basic_error(SYNTAX);
    //         return;
    //     }

    //     /*Get Comma unless we are up to the last argument*/
    //     if (iArgNum < (iNumArguments-1))
    //     {
    //         get_token();
    //         if (*psContext->acToken != ',') basic_error(SYNTAX);
    //     }
    // }

    // /*Get comma or bracket*/
    // get_token();

    // if (*psContext->acToken == ',')
    // {
    //     get_token();
    //     if (psContext->eTokenType != STRINGVARIABLE) basic_error(SYNTAX);

    //     iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
    //     RtnParm = psContext->asStringVariables[iIndex].acValue;
    //     str_size = sizeof(psContext->asStringVariables[iIndex].acValue);        
    // }
    // else
    // {
    //     putback();
    // }

    // get_Bracket(')');

    // ProcessGpibCommand(param[0], param[1], RtnParm, str_size);
}


/***************************************************************************
Function    :  basic_Relay
Description :  Relay Control
Returns     :  Nothing
***************************************************************************/
void basic_Relay()
{
    char acRelayArguments[2][100];
    const int iNumArguments = 2;
    int iArgNum;
    int iIndex;

    get_Bracket('(');

    /*Get Relay Arguments*/
    for(iArgNum = 0; iArgNum < iNumArguments; iArgNum++)
    {

        /*Get Argument*/
        get_token();

        if (psContext->eTokenType == STRINGVARIABLE)
        {
            /*String Variable*/
            iIndex = find_Variable(psContext->acToken, psContext->eTokenType);
            strcpy(&acRelayArguments[iArgNum][0], psContext->asStringVariables[iIndex].acValue);
        }
        else
        {
           /*Assume Literal String or quoted string*/
           strcpy(&acRelayArguments[iArgNum][0], psContext->acToken);
        }

        /*Get Comma unless we are up to the last argument*/
        if (iArgNum < (iNumArguments-1))
        {
            get_token();
            if (*psContext->acToken != ',') syntax_error(SYNTAX);
        }
    }

    get_Bracket(')');

    //SetRelay(acRelayArguments[0], acRelayArguments[1]);
    printf("Relay control not implemented\n");
}






/***************************************************************************
Function    :  basic_Retry
Description :
               
Returns     :  Nothing
***************************************************************************/
void basic_Retry(void)
{
    // int iTemp = 0;
  

    // get_Bracket('(');

    // /*Get Colour*/
    // get_token();

    // switch(psContext->eTokenType)
    // {

    // case STRINGVARIABLE:
    //     sscanf(get_StringVariable(psContext->acToken), "%d", &iTemp);
    //     break;

    // case NUMBER:
    //     sscanf(psContext->acToken, "%d", &iTemp);
    //     break;

    // default:
    //     putback();
    //     eval_IntegerExpression(&iTemp);
    //     break;

    // }

    // get_Bracket(')');

	// /*Set the Retry limit*/
    // iMaxRetries = iTemp;
   

} /*end basic_Retry*/




/***************************************************************************
Function    :  basic_InterCharDelay
Description :  Set the intercharacter delay for use in terminal mode
               This is useful for talking to software that cannot handle
               full speed communication.  Delay is in milliseconds.
Returns     :  Nothing
***************************************************************************/
void basic_InterCharDelay(void)
{
    // int iTemp = 0;
  

    // get_Bracket('(');

    // /*Get Colour*/
    // get_token();

    // switch(psContext->eTokenType)
    // {

    // case STRINGVARIABLE:
    //     sscanf(get_StringVariable(psContext->acToken), "%d", &iTemp);
    //     break;

    // case NUMBER:
    //     sscanf(psContext->acToken, "%d", &iTemp);
    //     break;

    // default:
    //     putback();
    //     eval_IntegerExpression(&iTemp);
    //     break;

    // }

    // get_Bracket(')');

	// /*Set the Intercharacter delay for use in terminal mode*/
    // iInterCharDelay = iTemp;
   

} /*end basic_InterCharDelay*/



/***************************************************************************
Function    :  basic_Terminal
Description :  Used for terminal emulation.
               Supports transmission and reception of ASCII text.
               
Returns     :  Nothing
***************************************************************************/
void basic_Terminal(void)
{
    // int iTemp = 0;
    // char acAsciiOut[1501];
    // char acAsciiIn[1501];
    // int iIndex;
    // char *pcTerminal;
    // int bSend = 0;
    
    // acAsciiOut[0] = 0;
    // acAsciiIn[0] = 0;

    // iIndex = find_Variable("terminal$", STRINGVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("terminal$", STRINGVARIABLE);
    // if(iIndex == -1)
    // {
    //     printf("FOOBAR!\n");
    //     return;
    //     //basic_error(OUT_OF_MEM);
    // }

    // get_Bracket('(');


    // // get text to transmit or closing bracket
    // get_token();

    // if (*psContext->acToken != ')')
    // {        


    //     switch(psContext->eTokenType)
    //     {

    //     case QUOTE:
    //     case STRINGVARIABLE:
	//     	putback();
    //         eval_StringExpression(acAsciiOut, 1500);
    //         break;

    //     default:
    //         putback();
    //         eval_IntegerExpression(&iTemp);
    //         sprintf(acAsciiOut, "%d", iTemp);
    //         break;
    //     }
    //     bSend = 1;
    //     get_Bracket(')');
    // }
    // else
    // {
    //     // toggle terminal mode on/off
    //     strcpy(acAsciiIn, "terminal");
    //     processCommand(acAsciiIn);
    //     bSend = 0;
    // }

    // if (bSend)
    // {
	//     //printf("AsciiOut (%d) = %s\n", strlen(acAsciiOut), acAsciiOut);

    //     if(!bTerminal)
    //     {
    //         // fire up terminal mode
    //          strcpy(acAsciiIn, "terminal");
    //          processCommand(acAsciiIn);
    //     }
        
    //     acAsciiIn[0] = 0;
        
    //     // add carriage return to output string so that it can be stripped when echoed 
    //     strcat(acAsciiOut, "\r");

    //     // send raw ASCII text
    //     if (!iInterCharDelay)
    //     {
    //         // write characters at full speed - most software can handle this
    //         writeRaw(acAsciiOut);
    //     }
    //     else
    //     {
    //         // for software that can't handle the pace add a delay between characters
    //         writeRawSuperSlow(acAsciiOut);  
    //     }

	//     // erase prompt and command line
    //     if(!bQuiet)
    //     {
	//         printf("\r                             \r");
    //     }

 	//     // clear reserved BASIC Variable ready for response
	//     pcTerminal = psContext->asStringVariables[iIndex].acValue;       
    //     *pcTerminal = 0;

    //     // get the response
    //     terminal_GetResponse(acAsciiOut, pcTerminal, STRING_VAR_LEN);
    // }

} /*end basic_Terminal*/




/***************************************************************************
Function    :  basic_Graph
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Graph(void)
{
//     int i;
//     int iUdpPort;
//     char acTemp[256];
    
//     acTemp[0] = 0;

//     /*Get the opening bracket*/
//     get_Bracket('(');

//    /*Get UDP port*/
//     get_token();
//     putback();
//     switch(psContext->eTokenType)
//     {

//         case QUOTE:
//         case STRINGVARIABLE:
//             eval_StringExpression(acTemp, 1500);
//             sscanf(acTemp, "%d", &iUdpPort);
//             break;

//         default:            
//             eval_IntegerExpression(&iUdpPort);            
//             break;
//     }
    

//     /*Now check if this is a kill command (no more parameters)*/
//     get_token();
//     if (strcmp(psContext->acToken, ")") == 0)
//     {
//         /*Close graph*/
//         sock_KillGraph(iUdpPort); 
//         return;
//     }
    
//     /*Begin creating text string to pass to command interpreter*/
//     strcpy(acTemp, "start Graph ");
//     sprintf(&acTemp[strlen(acTemp)], "%d ", iUdpPort); 

//     /*Get title, type, window position, window size, graph limits (10 parameters)*/
//     for(i = 0; i < 9; i++)
//     {
//         get_Parameter(acTemp);
//         strcat(acTemp, " ");
//         get_token();          //skip comma
//     }
    
//     get_Parameter(acTemp);

//     /*Get the closing bracket*/
//     get_Bracket(')');

//     /*Pass the command string to the interpreter for processing*/
//     //printf("Command line: %s\n", acTemp);

//     flushall();
//     system(acTemp);

//     update_SnmpVariables();

//     /*Attempt to reclaim focus from Rod's evil graph.exe*/
//     Sleep(200);
//     basic_SetFocus();

} /*end basic_Graph*/


/***************************************************************************
Function    :  basic_Plot
Description :
               mark string locations (from 0) inclusive
Returns     :  Nothing
***************************************************************************/
void basic_Plot(void)
{
//     int iUdpPort;
//     int x = 0;
//     int y = 0;
//     int z = 0;
//     char acTemp[1501];
  

//     get_Bracket('(');

//    /*Get UDP port*/
//     get_token();
//     putback();
//     switch(psContext->eTokenType)
//     {

//         case QUOTE:
//         case STRINGVARIABLE:
//             eval_StringExpression(acTemp, 1500);
//             sscanf(acTemp, "%d", &iUdpPort);
//             break;

//         default:            
//             eval_IntegerExpression(&iUdpPort);            
//             break;
//     }
    
//     /*Skip comma*/
//     get_token();  

//    /*Get X co-ordinate*/
//     get_token();
//     putback();
//     switch(psContext->eTokenType)
//     {

//         case QUOTE:
//         case STRINGVARIABLE:
//             eval_StringExpression(acTemp, 1500);
//             sscanf(acTemp, "%d", &x);
//             break;

//         default:            
//             eval_IntegerExpression(&x);            
//             break;
//     }

//     /*Skip comma*/
//     get_token(); 

//     /*Get Y co-ordinate*/
//     get_token();
//     putback();
//     switch(psContext->eTokenType)
//     {

//         case QUOTE:
//         case STRINGVARIABLE:
//             eval_StringExpression(acTemp, 1500);
//             sscanf(acTemp, "%d", &y);
//             break;

//         default:            
//             eval_IntegerExpression(&y);            
//             break;
//     }

//     /*Skip comma*/
//     get_token();    
//     if (*psContext->acToken == ',')
//     {

//         /*Get Z co-ordinate*/
//         get_token();
//         putback();
//         switch(psContext->eTokenType)
//         {

//             case QUOTE:
//             case STRINGVARIABLE:
//                 eval_StringExpression(acTemp, 1500);
//                 sscanf(acTemp, "%d", &z);
//                 break;

//             default:            
//                 eval_IntegerExpression(&z);            
//                 break;
//         }

//     }
//     else
//     {
//         putback();
//     }


//     get_Bracket(')');

//     /*Plot it!*/
//     sock_Plot(iUdpPort, x, y, z);
   

} /*end basic_Plot*/



/***************************************************************************
Function    :  basic_TrapLog
Description :  Controls trap logging function.               
Returns     :  Nothing
***************************************************************************/
void basic_TrapLog(void)
{
//     char acTemp[1501];
//     int qEnableLog = 0;
  

//     get_Bracket('(');

//    /*Get ON or OFF*/
//     get_token();
//     putback();
//     switch(psContext->eTokenType)
//     {

//         case QUOTE:
//         case STRINGVARIABLE:
//             eval_StringExpression(acTemp, 1500);
//             if (strcasecmp(acTemp, "OFF") == 0)
//             {
//                 // LOG ON
//                 qEnableLog = 0;
//             }
//             else
//             {
//                 // LOG OFF
//                 qEnableLog = 1;
//             }
//             break;

//         default:            
//             eval_IntegerExpression(&qEnableLog);            
//             break;
//     }
    

//     get_Bracket(')');

//     /*Enable or disable trap logging*/
//     sock_TrapLog(qEnableLog);
   

} /*end basic_TrapLog*/

/***************************************************************************
Function    :  update_SnmpVariables
Description :  The name says it all
Returns     :  nothing
***************************************************************************/
void update_SnmpVariables(void)
{
    int iIndex = 0;
    int iValue = 0;


    // /*Update the error status first*/
	// iIndex = find_Variable("errorstatus", FLOATVARIABLE);
    // if (iIndex == -1) iIndex = create_Variable("errorstatus", FLOATVARIABLE);
    // psContext->asFloatVariables[iIndex].fValue = iErrStat;
	
    // /*Convert SNMP Exceptions to errors - this simplifies error checking in scripts*/
    // if (strcmp(acLastValue, "noSuchObject") == 0)
    // {
    //     psContext->asFloatVariables[iIndex].fValue = noSuchObject;
    // }
    // else if (strcmp(acLastValue, "noSuchInstance") == 0)
    // {
    //     psContext->asFloatVariables[iIndex].fValue = noSuchInstance;
    // }


	// /*If an error occurs then set SNMP value to zero.
	//   This is to assist lazy script writers who may fail to check error status.*/
	// if (iErrStat != 0)
	// {
	// 	strcpy(acLastValue, "0");
	// }

	// /*Update SNMP reserved Variables with SNMP response results*/
	// iIndex = find_Variable("value$", STRINGVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("value$", STRINGVARIABLE);
	// strcpy(psContext->asStringVariables[iIndex].acValue, acLastValue);

	// iIndex = find_Variable("oid$", STRINGVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("oid$", STRINGVARIABLE);
	// strcpy(psContext->asStringVariables[iIndex].acValue, pacLastName);

	// iIndex = find_Variable("instance$", STRINGVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("instance$", STRINGVARIABLE);
	// strcpy(psContext->asStringVariables[iIndex].acValue, acReturnedInstance);

	// iIndex = find_Variable("octetstring$", STRINGVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("octetstring$", STRINGVARIABLE);
	// strcpy(psContext->asStringVariables[iIndex].acValue, acLastOctetString);

	// iIndex = find_Variable("value", FLOATVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("value", FLOATVARIABLE);
	// sscanf(acLastValue, "%d", &iValue);
    // psContext->asFloatVariables[iIndex].fValue = (double)iValue;

	// iIndex = find_Variable("octetstringsize", FLOATVARIABLE);
	// if (iIndex == -1) iIndex = create_Variable("octetstringsize", FLOATVARIABLE);
	// psContext->asFloatVariables[iIndex].fValue = (double)LastOctetStringLength;

}


/***************************************************************************
Function    :  get_Parameter
Description :  Gets the next parameter and appends it to the command
               string.  This is for use with commands that create a command
               line string and then call the command line parser.
Returns     :  nothing
***************************************************************************/
void get_Parameter(char *pcCommandLine)
{
    char acTemp[STRING_VAR_LEN];
    int x;
    

    acTemp[0] = 0;

    if (pcCommandLine == NULL)
    {
        printf("Internal Error: NULL command line pointer in get_Parameter\n");
        return;
    }

    /*Skip comma if present*/
    do
    {
        get_token();        
    } while(*psContext->acToken == ',');

        
    putback();
    switch(psContext->eTokenType)
    {

        case QUOTE:
        case STRINGVARIABLE:
            eval_StringExpression(acTemp, STRING_VAR_LEN-1);
            break;

        default:            
            eval_IntegerExpression(&x); 
            sprintf(acTemp, "%d", x);
            break;
    }
    
    strcat(pcCommandLine, acTemp);
    
}


/***************************************************************************
Function    :  basic_SetFocus
Description :  Attempts to gain focus for the SNMP Window.
               This can be used to ensure a script can accept keyboard input.
Returns     :  nothing
***************************************************************************/
void basic_SetFocus(void)
{
    // HWND hWnd = NULL;                           /* handle of this window */
    // HWND hWndFocus = NULL;                      /* handle of focus window */
    // HWND hWndActive = NULL;                     /* handle of active window */
    // int bHidden = 0;
 
      

    // /*Get a handle to the Window.  This is unreliable under Windows we must retry.*/
    // hWnd = FindWindow( "ConsoleWindowClass", SNMP_VERSION);
    // if (!hWnd)
    // {
    //     for (bHidden = 10; bHidden > 0; bHidden--)
    //     {
    //         Sleep(200);
    //         hWnd = FindWindow( "ConsoleWindowClass", SNMP_VERSION);
    //         if (hWnd)
    //         {
    //             /*Got the handle.  Thanks for nothing Bill, you utter #@#$%!!!*/
    //             break;
    //         }
    //     }
    // }

    // if (!hWnd) printf("SNMP failed to find window handle\n");

    // /*Gain focus for us but restore graph to front of screen*/
    // if (hWnd != NULL)
    // {
    //     hWndFocus = GetForegroundWindow();
    //     hWndActive = GetActiveWindow();

    //     BringWindowToTop(hWnd);
    //     SetForegroundWindow(hWnd);
    //     if (hWndActive != NULL)
    //         BringWindowToTop(hWndActive);
    //     if (hWndFocus != NULL)
    //         BringWindowToTop(hWndFocus);
        
    //     //if (hWndActive == NULL)
    //     //       SetForegroundWindow(hWndFocus);    
    // }   
}



/***************************************************************************
Function    :  basic_ShellyGet
Description :  Get shelly parameter value
Returns     :  Nothing
***************************************************************************/
void basic_ShellyGet(void)
{
    int value = 0;
    char device_ip_string[32];
    char parameter_name[32];
    int iIndex = -1;
    uint8_t device_ip[4] = {0,0,0,0};

    /*Get the opening bracket*/
    get_Bracket('(');

    // ip address
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        strncpy(device_ip_string, get_StringVariable(psContext->acToken), sizeof(device_ip_string));
        break;
        case DELIMITER:
        case INTEGERVARIABLE:
        case COMMAND:
        case LABEL:
        case FLOATVARIABLE:
        case FUNCTION:
        case LOGIC:
        case USERFUNCTION:
            syntax_error(SYNTAX);
            break;
    default:
    case QUOTE:    
    case NUMBER:
    case STRING:
        strncpy(device_ip_string, psContext->acToken, sizeof(device_ip_string));
        break;
    }

    // comma
    get_token();

    // parameter name
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        strncpy(parameter_name, get_StringVariable(psContext->acToken), sizeof(parameter_name));
        break;
        case DELIMITER:
        case INTEGERVARIABLE:
        case NUMBER:
        case COMMAND:
        case LABEL:
        case FLOATVARIABLE:
        case FUNCTION:
        case LOGIC:
        case USERFUNCTION:
            syntax_error(SYNTAX);
            break;
    default:
    case QUOTE:    
    case STRING:
        strncpy(parameter_name, psContext->acToken, sizeof(parameter_name));
        break;
    }

    // comma
    get_token();

    // return variable
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        iIndex = find_Variable(psContext->acToken, STRINGVARIABLE);
	    if (iIndex == -1) iIndex = create_Variable(psContext->acToken, STRINGVARIABLE);
        break;
    default:
        syntax_error(SYNTAX);
        break;
    }

    /*Get the closing bracket*/
    get_Bracket(')');

    // convert ip address string to 4 bytes
    ip_string_to_int_array_pton(device_ip_string, device_ip); 

    if(iIndex >=0 )
    {
        if (shelly_cache_get_value(device_ip, parameter_name, psContext->asStringVariables[iIndex].acValue, sizeof(psContext->asStringVariables[iIndex].acValue)))
        {
            printf("RUNTIME ERROR: shelly_get failed (%s)\n", psContext->asStringVariables[iIndex].acValue);
            psContext->asStringVariables[iIndex].acValue[0] = 0;    
        }
    }

} /*End basic_ShellyGet*/


/***************************************************************************
Function    :  basic_ShellySwitch
Description :  Set shelly relay state  template: shelly_switch(192.168.33.180, 0, "ON")
Returns     :  Nothing
***************************************************************************/
void basic_ShellySwitch(void)
{
    int err = 0;
    int value = 0;
    char device_ip_string[32];
    char relay_string[32];
    char state_string[32];    
    char command_string[32];       
    int relay = 0;
    int iIndex = -1;
    uint8_t device_ip[4] = {0,0,0,0};

    /*Get the opening bracket*/
    get_Bracket('(');

    // ip address
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        strncpy(device_ip_string, get_StringVariable(psContext->acToken), sizeof(device_ip_string));
        break;
        case DELIMITER:
        case INTEGERVARIABLE:        
        case COMMAND:
        case LABEL:
        case FLOATVARIABLE:        
        case FUNCTION:
        case LOGIC:
        case USERFUNCTION:
            syntax_error(SYNTAX);
            break;
    default:
    case QUOTE:    
    case NUMBER:
    case STRING:
        strncpy(device_ip_string, psContext->acToken, sizeof(device_ip_string));
        break;
    }

    // comma
    get_token();

    // relay number
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        strncpy(relay_string, get_StringVariable(psContext->acToken), sizeof(relay_string));
        break;
        case DELIMITER:
        case COMMAND:
        case LABEL:
        case FUNCTION:
        case LOGIC:
        case USERFUNCTION:
            syntax_error(SYNTAX);
            break;
        case INTEGERVARIABLE: 
            relay = get_IntegerVariable(psContext->acToken);
            snprintf(relay_string, sizeof(relay_string), "%d", relay);
            break;
        case FLOATVARIABLE:
            relay = (int)round(get_FloatVariable(psContext->acToken));
            snprintf(relay_string, sizeof(relay_string), "%d", relay);             
            break;  
        case NUMBER:
            strncpy(relay_string, psContext->acToken, sizeof(relay_string));                      
            break;
    default:
    case QUOTE:    
    case STRING:
        strncpy(relay_string, psContext->acToken, sizeof(relay_string));  //TODO: strip quotes -- add in-place modification function to utility.c
        break;
    }

    // comma
    get_token();

    // state -- on or off
    get_token();
    
    switch(psContext->eTokenType)
    {
    case STRINGVARIABLE:
        strncpy(state_string, get_StringVariable(psContext->acToken), sizeof(state_string));
        break;
        case DELIMITER:
        case INTEGERVARIABLE:        
        case COMMAND:
        case LABEL:
        case FLOATVARIABLE:        
        case FUNCTION:
        case LOGIC:
        case USERFUNCTION:
            syntax_error(SYNTAX);
            break;
    default:
    case QUOTE:    
    case NUMBER:
    case STRING:
        strncpy(state_string, psContext->acToken, sizeof(state_string));
        break;
    }

    /*Get the closing bracket*/
    get_Bracket(')');

    // construct shelly command
    snprintf(command_string, sizeof(command_string), "/relay/%s?turn=%s", relay_string, state_string);

    shelly_http_request(HTTP_GET, command_string, device_ip_string, NULL);
} /*End basic_ShellySwitch*/

 
