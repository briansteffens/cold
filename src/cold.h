
typedef enum { false, true } bool;

enum VarType
{
    TYPE_INT
,   TYPE_FLOAT
,   TYPE_STRING
};

enum ParamType
{
    PARAM_LABEL
,   PARAM_LITERAL
,   PARAM_PATTERN
};

enum InstructionType
{
    INST_LET
,   INST_ADD
,   INST_MUL
,   INST_JUMP
,   INST_CMP
,   INST_RET
,   INST_PRINT
,   INST_NEXT
};

enum Patterns
{
    PTRN_LOCALS = 1
,   PTRN_CONSTANTS = 2
};

char* var_type_tostring(enum VarType input);
enum VarType var_type_fromstring(char* input);

char* param_type_tostring(enum ParamType input);

char* instruction_type_tostring(enum InstructionType input);
enum InstructionType instruction_type_fromstring(char* input);

struct Value
{
    enum VarType type;
    void* data;
    int size;
};

void value_tostring(struct Value* val, char* buf, int n);
void value_set_string(struct Value* value, char* val);
void value_set_int(struct Value* value, int val);
void value_set_float(struct Value* value, float val);
void value_set_from_string(struct Value* value, char* input);

struct Local
{
    char* name;
    struct Value* value;
};

struct Param
{
    enum ParamType type;
    struct Value* value;
};

void param_tostring(struct Param* p, char* buf, int n);

struct Instruction
{
    enum InstructionType type;
    struct Param** params;
    int param_count;
    int pattern_depth;
};

void params_allocate(struct Instruction* inst, int param_count);

void instruction_tostring(struct Instruction* input, char* buf, int n);

struct State
{
    struct Local** locals;
    bool* locals_owned;
    int local_count;

    struct Instruction** instructions;
    bool* instructions_owned;
    int instruction_count;

    int inst_ptr;

    struct Value* ret;
};

struct StateSet
{
    struct State** states;
    int state_count;
};

struct Case
{
    struct Value* input_values;
    struct Value expected;
};

struct Pattern
{
    struct Instruction** insts;
    int inst_count;
};

struct Context
{
    char** input_names;
    int input_count;

    struct Case* cases;
    int case_count;

    struct Instruction** solution_inst;
    int solution_inst_count;

    struct Value precision;

    int depth;

    struct Pattern** patterns;
    int pattern_count;
    bool** pattern_mask;

    struct Value* constants;
    int constant_count;

    char* generated_programs_filename;
};

struct Argument
{
    char* name;
    enum VarType type;
};

struct Function
{
    char* name;

    struct Argument* args;
    int arg_count;

    struct Instruction* insts;
    int inst_count;
};

void free_function(struct Function* func);
