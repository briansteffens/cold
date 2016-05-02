#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum { false, true } bool;

typedef enum ValueTypes
{
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_LONG_DOUBLE,
    TYPE_STRING
} ValueType;

typedef enum ParamTypes
{
    PARAM_LABEL,
    PARAM_LITERAL,
    PARAM_SUBSTITUTION
} ParamType;

typedef enum InstructionTypes
{
    INST_LET,
    INST_ADD,
    INST_MUL,
    INST_DIV,
    INST_EXP,
    INST_JUMP,
    INST_CMP,
    INST_RET,
    INST_PRINT,
    INST_NEXT
} InstructionType;

typedef enum PatternValues
{
    PTRN_LOCALS = 1,
    PTRN_CONSTANTS = 2
} Patterns;

typedef struct Value
{
    ValueType type;
    void* data;
    int size;
} Value;

typedef struct Case
{
    Value* input_values;
    Value expected;
} Case;

typedef struct Param
{
    ParamType type;
    Value* value;
} Param;

typedef struct Instruction
{
    InstructionType type;
    Param** params;
    int param_count;
} Instruction;

typedef struct Pattern
{
    Instruction** insts;
    int inst_count;
} Pattern;

typedef struct Context
{
    char** input_names;
    int input_count;

    Case* cases;
    int case_count;

    Instruction** solution_inst;
    int solution_inst_count;

    Value precision;

    int depth;

    Pattern** patterns;
    int pattern_count;

    Value* constants;
    int constant_count;

    char* generated_programs_filename;
    unsigned long* programs_completed;

    bool print_solutions;
    bool find_all_solutions;

    char* solution_fn;
} Context;

typedef struct Local
{
    char* name;
    Value* value;
} Local;

typedef struct State
{
    Local** locals;
    bool* locals_owned;
    int local_count;

    Instruction** instructions;
    bool* instructions_owned;
    int instruction_count;

    int inst_ptr;

    Value* ret;
} State;

typedef struct Function
{
    char* name;

    char** args;
    int arg_count;

    Instruction* insts;
    int inst_count;
} Function;

typedef struct Combination
{
    Instruction** instructions;
    int instruction_count;
} Combination;

char* var_type_tostring(ValueType input);
ValueType var_type_fromstring(char* input);

char* param_type_tostring(ParamType input);

char* instruction_type_tostring(InstructionType input);
InstructionType instruction_type_fromstring(char* input);

void value_tostring(Value* val, char* buf, int n);
void value_set_string(Value* value, char* val);
void value_set_int(Value* value, int val);
void value_set_float(Value* value, float val);
void value_set_long_double(Value* value, long double val);
void value_set_from_string(Value* value, char* input);
Value* value_clone(const Value* src);
bool compare(Context* ctx, Value* left, Value* right);
void value_free(Value* value);

void param_tostring(Param* p, char* buf, int n);

void params_allocate(Instruction* inst, int param_count);

void instruction_tostring(Instruction* input, char* buf, int n);
void instruction_free(Instruction* inst);
Instruction* instruction_clone(Instruction* orig);

void function_free(Function* func);

void combination_free(Combination* combination);

void pattern_free(Pattern* pattern);

void local_free(Local* local);

void state_free(State* state);

void print_program(Instruction** inst, int count, bool line_nums);
