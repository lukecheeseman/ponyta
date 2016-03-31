#include "../evaluate/evaluate.h"

//TODO: preferably with errorframes
ast_t* evaluate_create_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_add_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_sub_int(ast_t* receiver, ast_t* args);

// casting methods
ast_t* evaluate_i8_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_i16_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_i32_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_i64_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_i128_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_ilong_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_isize_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_u8_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_u16_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_u32_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_u64_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_u128_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_ulong_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_usize_int(ast_t* receiver, ast_t* args);
