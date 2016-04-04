#include "../evaluate/evaluate.h"

//TODO: preferably with errorframes
ast_t* evaluate_create_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_add_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_sub_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_mul_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_div_int(ast_t* receiver, ast_t* args);

ast_t* evaluate_neg_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_eq_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_ne_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_lt_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_le_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_ge_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_gt_int(ast_t* receiver, ast_t* args);

ast_t* evaluate_min_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_max_int(ast_t* receiver, ast_t* args);

ast_t* evaluate_hash_int(ast_t* receiver, ast_t* args);

ast_t* evaluate_and_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_or_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_xor_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_not_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_shl_int(ast_t* receiver, ast_t* args);
ast_t* evaluate_shr_int(ast_t* receiver, ast_t* args);

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
