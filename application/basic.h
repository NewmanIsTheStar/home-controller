/******************************************************************************
File:		basic.h
Description:BASIC defintions.
******************************************************************************/

#ifndef BASIC_H
#define BASIC_H

// extracted from snmp_def.h
enum keys
{
	F1 = 315, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11 =389, F12, ESC = 27, SPACE = 32, HOMEKEY = 327,
	ENDKEY =335, ENTER = 13, BACKSPACE = 8, DOT = 46, MINUS = 45, PLUS = 43, CURSOR_LEFT = 331,
	CURSOR_RIGHT = 333, CURSOR_UP = 328, CURSOR_DOWN = 336,TAB = 9, SHIFTTAB = 271, DELETEKEY = 339,
	CTRL_Y = 25, CTRL_T = 20, CTRL_Q = 17, CTRL_BACKSPACE = 127, CTRL_LEFT = 372, CTRL_RIGHT = 371,
	CTRL_C = 3, CTRL_V = 22, CTRL_TAB = 404
};

/*Public Definitons*/
#define NUM_LAB               (100)             //Maximum Number of labels
#define LAB_LEN               (30)              //Maximum length of a label
#define NUM_FUNC              (100)             //Maximum Number of user functions
#define FUNC_LEN              (30)              //Maximum length of a function
#define FOR_NEST              (25)              //Maximum FOR nesting depth
#define WHILE_NEST            (25)              //Maximum WHILE nesting depth
#define SUB_NEST              (25)              //Maximum GOSUB nesting depth
#define PROG_SIZE             (131072)          //Maximum Program Size
#define BASIC_RECURSION_DEPTH (20)              //Maximum Program nesting depth
#define NUM_INTEGER_VARIABLES (200)             //Maximum number of integer variables
#define NUM_FLOAT_VARIABLES   (200)             //Maximum number of float variables
#define NUM_STRING_VARIABLES  (120)             //Maximum number of string variables
#define NUM_ARRAY_VARIABLES   (100)             //Maximum number of array variables
#define STRING_VAR_LEN        (1500)            //Maximum length of a string variable
#define VAR_NAME_LEN          (30)              //Maximum length of a variable name
#define NUM_SHARED_VARIABLES  (20)
#define NUM_COMMON_VARIABLES  (20)
#define MAX_ARRAY_DIM         (4)

typedef enum 
{
    DELIMITER,      INTEGERVARIABLE,   NUMBER,     COMMAND,
    STRING,         QUOTE,      LABEL,      STRINGVARIABLE,
	FLOATVARIABLE,  FUNCTION,   LOGIC,      USERFUNCTION
} teTokenType;

typedef enum
{
    ABS = 1,        ASC,            ATOI,       ATOIHEX,
    BEEP,           BER_RESET,      CALL,       CHAIN,
    CHR$,           CLEAR,          CLOSE,      CLS,
    COMMON,         CONTEXTID,      DELAY,      DUMP,
    ELSE,           ELSEIF,         END,        EOL,
    FINISHED,       FOR,            GET,        GETNEXT,
    GOSUB,          GOTO,           GPIB_CMD,   HEX$,
    HOME,           IF,             INPUT,      INPUT_FROM_FILE,
    INSTR,          ITOAHEX,        LEFT$,      LEN,
    LET,            LOG,            MERGE,      MID,
    NEXT,           ON,             OPEN,       PRINT,
    PRINT_TO_FILE,  QUIET,          RASADDR,    RELAY,
    REM,            RETURN,         RIGHT$,     RUN,
    SET,            SHARED,         STR$,       STEP,
    STOP,           SUB,            SYSTEM,     TARGET,
    TFTP,           THEN,           TIMEOUT,    TIMESTAMP,
    TO,             TRACE,          UCASE$,     VAL,
    VERBOSE,        WEND,           WHILE,      COLOUR,
	RETRY,          TERMINAL,       GRAPH,      PLOT,
    CURSORXY,       TRAPS,          DEF_FUNC,   DIM,
    SLEEP,          SCREEN_SAVE,    SCREEN_RESTORE,
    COM,
	
    LEN$,
    
    ACOS,           ASIN,           ATAN,       COS,
    COSH,           LN,             SIN,        SINH,
    SQRT,           TAN,            TANH,       INTERCHARDEALY,

    AND,            OR,             NOT,

    SHELLY_GET,
    
    NO_MORE_TOKENS
} teToken;


typedef struct
{
    char name[LAB_LEN];                     //label
    char *p;                                //points to label's location in source file
} tsLabel;



typedef struct 
{
    teTokenType eCounterType;               //counter variable type 
    int iVariableIndex;                     //counter variable index
    double fTarget;                         //target value
    double fStepSize;                       //added to the loop variable each iteration
    char *loc;                              //place in source code to loop to
} tsForStack; 

typedef struct 
{
    char *loc;                              //place in source code to loop to
} tsWhileStack; 

typedef struct 
{
    char acName[VAR_NAME_LEN];              //variable name
    int iValue;                             //variable value
} tsIntegerVariable; 

typedef struct 
{
    char acName[VAR_NAME_LEN];              //variable name
    double fValue;                          //variable value
} tsFloatVariable;

typedef struct 
{
    char acName[VAR_NAME_LEN];               //variable name
    char acValue[STRING_VAR_LEN];            //variable value
} tsStringVariable;

typedef struct 
{
    char acName[VAR_NAME_LEN];               //variable name
    teTokenType eType;                       //varibale type
    int iDimensions[MAX_ARRAY_DIM];          //dimension sizes
    int iMultiplier[MAX_ARRAY_DIM];          //dimension multipliers
    char *pcValue;                           //point to array
} tsArrayVariable;

typedef struct 
{
    teTokenType       eParamType;
    tsIntegerVariable sParamInt;
    tsFloatVariable   sParamFlt;  
    tsStringVariable  sParamStr;
} tsFunctionParameter;

typedef struct
{
    char name[FUNC_LEN];                    //function
    char *p;                                //points to function's location in source file
    tsFunctionParameter asParameters[10];   //parameter list
} tsUserFunction;

typedef struct
{
    char acToken[STRING_VAR_LEN];               //Current token string
    teTokenType eTokenType;                     //Current token type
    teToken eToken;                             //Current token
    char *pcProgramCounter;                     //points into the program
    char *pcProgram;                            //points to start of program
    jmp_buf sEnviroment;                        //holds environment for longjmp()
    tsForStack sForStack[FOR_NEST];             //nested FOR loop information
    tsWhileStack sWhileStack[WHILE_NEST];       //nested WHILE loop information
    char *cGosubStack[SUB_NEST];	            //stack for gosub
    int iTopOfForStack;                         //index to top of FOR stack
    int iTopOfWhileStack;                       //index to top of WHILE stack
    int iTopOfGosubStack;                       //index to top of GOSUB stack
    int iThenElseLine;                          //counter used to find blockto execute
    FILE *apFileHandles[11];                    //handles to files opened by user
    tsLabel sLabelTable[NUM_LAB];               //all labels found in script
    tsUserFunction sUserFunctionTable[NUM_FUNC];//all user functions found in script
    tsIntegerVariable asIntegerVariables[NUM_INTEGER_VARIABLES];
	tsFloatVariable asFloatVariables[NUM_FLOAT_VARIABLES];
    tsStringVariable asStringVariables[NUM_STRING_VARIABLES];
    tsArrayVariable asArrayVariables[NUM_ARRAY_VARIABLES];
    char acSharedVariables[NUM_SHARED_VARIABLES][VAR_NAME_LEN];  
    char acCommonVariables[NUM_COMMON_VARIABLES][VAR_NAME_LEN]; 
    char acFileName[256];
} tsBasicContext;


typedef struct
{
    char acName[20];                   //Command name in lower case
    teToken eToken;                    //Command token
    void (*pfCommand)(void);           //Command function pointer
} tsCommand;

typedef struct
{
    char acName[20];                   //Function name in lower case
    teToken eToken;                    //Function token
} tsFunction;

/* Used to call basic_error() when a syntax error occurs.*/
enum error_msg
{
    SYNTAX,         UNBAL_PARENS,   NO_EXP,         EQUALS_EXP,
    NOT_VAR,        LAB_TAB_FULL,   DUP_LAB,        UNDEF_LAB,
    FUNC_TAB_FULL,  DUP_FUNC,       UNDEF_FUNC,
    THEN_EXP,       TO_EXP,         TOO_MNY_FOR,    NEXT_WO_FOR,
    TOO_MNY_WHILE,  WEND_WO_WHILE,  TOO_MNY_GOSUB,  RET_WO_GOSUB,
    MISS_QUOTE,     MISS_PARENS,    OUT_OF_MEM,     NO_ENDIF,
    TOO_MNY_ELSE,   UNIMPLEMENTED
};

/*BASIC command prototypes*/
void basic_Print(void);
void find_endiforelse(void),    basic_Goto(void),       basic_If(void);
void basic_Else(void),          basic_For(void),        basic_ElseIf(void);
void basic_Next(void),          fpush(tsForStack i),    basic_Input(void);
void basic_Gosub(void),         basic_Return(void),     gpush(char *s);
void basic_Let(void),           basic_Get(void),        basic_Dim(void);
void basic_Set(void),           basic_Rem(void),        basic_GetNext(void);
void basic_Quiet(void),         basic_Tftp(void),       basic_ContextId(void);
void basic_Target(void),        basic_Verbose(void),    basic_LogToFile(void);
void basic_TimeOut(void),       basic_Delay(void),      basic_System(void);
void basic_Home(void),          basic_Mid(void),        basic_AtoI(void);
void basic_AtoIHex(void),       basic_Gpib(void),       basic_Relay(void);
void basic_Open(void),          basic_Close(void),      basic_PrintToFile(void);
void basic_TimeStamp(void),     basic_Error(void),      basic_InputFromFile(void);
void basic_Ignore(void),        basic_End(void),        basic_Run(void);
void basic_Beep(void),          basic_On(void),         basic_Shared(void);
void basic_Common(void),        basic_Trace(void),      basic_RasAddr(void);
void basic_Colour(void),        basic_Retry(void),		basic_Len(void);
void basic_Ucase(void),         basic_Left(void),		basic_Right(void);
void basic_Graph(void),         basic_Plot(void),       basic_Terminal(void);
void basic_ItoAHex(void),       basic_Cls(void),        basic_InterCharDelay(void);
void basic_CursorXY(void),      basic_TrapLog(void),    basic_While(void);
void basic_Wend(void),          basic_Chain(void),      basic_Function(void);
void basic_Sleep(void),         basic_SaveScreen(void), basic_RestoreScreen(void);
void basic_Com(void);

/*Other prototypes*/
void update_SnmpVariables(void),dump_Variables(void);
void get_Bracket(char cBracket),basic_NotImplemented(void);
int basic_CreateContext(char *pcFileName, char *pcArguments);
int basic_DestroyContext(void);
int load_program(char *p, char *fname);
void find_eol(void);
void label_init(void);
void scan_labels(void);
int get_next_label(char *s);
char *find_label(char *s);
void userFunction_init(void);
void scan_UserFunctions(void);
int get_next_UserFunction(char *s);
char *find_UserFunction(char *s);
char *gpop(void);
char *get_StringVariable(char *pcName);
int find_Variable(char *pcName, teTokenType eTokenType);
int create_Variable(char *pcName, teTokenType eTokenType);
tsForStack fpop(void);
void wpush(tsWhileStack i);
tsWhileStack wpop(void);
void update_DateAndTime(void);
int basic_Interpreter(char *pcFileName, char *pcArguments);

int get_token(void);
int next_token(void);
void syntax_error(int error);
void putback(void);
void eval_NumericExpression(int *piAnswer, double *pfAnswer);
void eval_IntegerExpression(int *piAnswer);
void eval_FloatExpression(double *pfAnswer);
void eval_StringExpression(char *pcResult, int nResultLen);
int get_IntegerVariable(char *pcName);
double get_FloatVariable(char *pcName);
char *get_StringVariable(char *pcName);
void get_Bracket(char cBracket);
int basic_InterpretFunction(char *pcFunctionBody, int iFuncNum);
int set_StringVariable(char *pcName, char *pcValue);
int set_IntegerVariable(char *pcName, int iValue);
int set_FloatVariable(char *pcName, double fValue);
char *find_ArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM]);
int get_IntegerArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM]);
int set_IntegerArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], int iValue);
double get_FloatArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM]);
int set_FloatArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], double fValue);
char *get_StringArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM]);
int set_StringArrayVariable(char *pcName, int aiIndex[MAX_ARRAY_DIM], char *pcValue);
int string_atom(char *pcAnswer, int iLength);
void eval_StringLine(char *pcResult, int nResultLen, int iProcessEol);
int eval_StringLogic(void);


#endif
