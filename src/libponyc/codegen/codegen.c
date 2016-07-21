#include "codegen.h"
#include "genlib.h"
#include "genexe.h"
#include "genprim.h"
#include "genname.h"
#include "gendesc.h"
#include "gencall.h"
#include "genopt.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"

#include <platform.h>
#include <llvm-c/Initialization.h>
#include <string.h>
#include <assert.h>

struct compile_local_t
{
  const char* name;
  LLVMValueRef alloca;
};

static size_t compile_local_hash(compile_local_t* p)
{
  return ponyint_hash_ptr(p->name);
}

static bool compile_local_cmp(compile_local_t* a, compile_local_t* b)
{
  return a->name == b->name;
}

static void compile_local_free(compile_local_t* p)
{
  POOL_FREE(compile_local_t, p);
}

DEFINE_HASHMAP(compile_locals, compile_locals_t, compile_local_t,
  compile_local_hash, compile_local_cmp, ponyint_pool_alloc_size,
  ponyint_pool_free_size, compile_local_free);

static void fatal_error(const char* reason)
{
  printf("LLVM fatal error: %s\n", reason);
}

static compile_frame_t* push_frame(compile_t* c)
{
  compile_frame_t* frame = POOL_ALLOC(compile_frame_t);
  memset(frame, 0, sizeof(compile_frame_t));
  compile_locals_init(&frame->locals, 0);

  if(c->frame != NULL)
    frame->prev = c->frame;

  c->frame = frame;
  return frame;
}

static void pop_frame(compile_t* c)
{
  compile_frame_t* frame = c->frame;
  compile_locals_destroy(&frame->locals);

  c->frame = frame->prev;
  POOL_FREE(compile_frame_t, frame);
}

static LLVMTargetMachineRef make_machine(pass_opt_t* opt)
{
  LLVMTargetRef target;
  char* err;

  if(LLVMGetTargetFromTriple(opt->triple, &target, &err) != 0)
  {
    errorf(opt->check.errors, NULL, "couldn't create target: %s", err);
    LLVMDisposeMessage(err);
    return NULL;
  }

  LLVMCodeGenOptLevel opt_level =
    opt->release ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;

  LLVMRelocMode reloc =
    (opt->pic || opt->library) ? LLVMRelocPIC : LLVMRelocDefault;

  LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, opt->triple,
    opt->cpu, opt->features, opt_level, reloc, LLVMCodeModelDefault);

  if(machine == NULL)
  {
    errorf(opt->check.errors, NULL, "couldn't create target machine");
    return NULL;
  }

  return machine;
}

static void init_runtime(compile_t* c)
{
  c->str_builtin = stringtab("$0");
  c->str_Bool = stringtab("Bool");
  c->str_I8 = stringtab("I8");
  c->str_I16 = stringtab("I16");
  c->str_I32 = stringtab("I32");
  c->str_I64 = stringtab("I64");
  c->str_I128 = stringtab("I128");
  c->str_ILong = stringtab("ILong");
  c->str_ISize = stringtab("ISize");
  c->str_U8 = stringtab("U8");
  c->str_U16 = stringtab("U16");
  c->str_U32 = stringtab("U32");
  c->str_U64 = stringtab("U64");
  c->str_U128 = stringtab("U128");
  c->str_ULong = stringtab("ULong");
  c->str_USize = stringtab("USize");
  c->str_F32 = stringtab("F32");
  c->str_F64 = stringtab("F64");
  c->str_Pointer = stringtab("Pointer");
  c->str_Maybe = stringtab("MaybePointer");
  c->str_Array = stringtab("Array");
  c->str_String = stringtab("String");
  c->str_Platform = stringtab("Platform");
  c->str_Main = stringtab("Main");
  c->str_Env = stringtab("Env");
  c->str_Vector = stringtab("Vector");
  c->str_Matrix = stringtab("Matrix");

  c->str_add = stringtab("add");
  c->str_sub = stringtab("sub");
  c->str_mul = stringtab("mul");
  c->str_div = stringtab("div");
  c->str_mod = stringtab("mod");
  c->str_neg = stringtab("neg");
  c->str_and = stringtab("op_and");
  c->str_or = stringtab("op_or");
  c->str_xor = stringtab("op_xor");
  c->str_not = stringtab("op_not");
  c->str_shl = stringtab("shl");
  c->str_shr = stringtab("shr");
  c->str_eq = stringtab("eq");
  c->str_ne = stringtab("ne");
  c->str_lt = stringtab("lt");
  c->str_le = stringtab("le");
  c->str_ge = stringtab("ge");
  c->str_gt = stringtab("gt");

  c->str_this = stringtab("this");
  c->str_create = stringtab("create");
  c->str__create = stringtab("_create");
  c->str__init = stringtab("_init");
  c->str__final = stringtab("_final");
  c->str__event_notify = stringtab("_event_notify");

  LLVMTypeRef type;
  LLVMTypeRef params[5];
  LLVMValueRef value;

  c->void_type = LLVMVoidTypeInContext(c->context);
  c->ibool = LLVMInt8TypeInContext(c->context);
  c->i1 = LLVMInt1TypeInContext(c->context);
  c->i8 = LLVMInt8TypeInContext(c->context);
  c->i16 = LLVMInt16TypeInContext(c->context);
  c->i32 = LLVMInt32TypeInContext(c->context);
  c->i64 = LLVMInt64TypeInContext(c->context);
  c->i128 = LLVMIntTypeInContext(c->context, 128);
  c->f32 = LLVMFloatTypeInContext(c->context);
  c->f64 = LLVMDoubleTypeInContext(c->context);
  c->intptr = LLVMIntPtrTypeInContext(c->context, c->target_data);

  // i8*
  c->void_ptr = LLVMPointerType(c->i8, 0);

  // forward declare object
  c->object_type = LLVMStructCreateNamed(c->context, "__object");
  c->object_ptr = LLVMPointerType(c->object_type, 0);

  // padding required in an actor between the descriptor and fields
  c->actor_pad = LLVMArrayType(c->i8, PONY_ACTOR_PAD_SIZE);

  // message
  params[0] = c->i32; // size
  params[1] = c->i32; // id
  c->msg_type = LLVMStructCreateNamed(c->context, "__message");
  c->msg_ptr = LLVMPointerType(c->msg_type, 0);
  LLVMStructSetBody(c->msg_type, params, 2, false);

  // trace
  // void (*)(i8*, __object*)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  c->trace_type = LLVMFunctionType(c->void_type, params, 2, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // serialise
  // void (*)(i8*, __object*, i8*, intptr, i32)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  params[2] = c->void_ptr;
  params[3] = c->intptr;
  params[4] = c->i32;
  c->serialise_type = LLVMFunctionType(c->void_type, params, 5, false);
  c->serialise_fn = LLVMPointerType(c->serialise_type, 0);

  // dispatch
  // void (*)(i8*, __object*, $message*)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  params[2] = c->msg_ptr;
  c->dispatch_type = LLVMFunctionType(c->void_type, params, 3, false);
  c->dispatch_fn = LLVMPointerType(c->dispatch_type, 0);

  // void (*)(__object*)
  params[0] = c->object_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(c->void_type, params, 1, false), 0);

  // descriptor, opaque version
  // We need this in order to build our own structure.
  const char* desc_name = genname_descriptor(NULL);
  c->descriptor_type = LLVMStructCreateNamed(c->context, desc_name);
  c->descriptor_ptr = LLVMPointerType(c->descriptor_type, 0);

  // field descriptor
  // Also needed to build a descriptor structure.
  params[0] = c->i32;
  params[1] = c->descriptor_ptr;
  c->field_descriptor = LLVMStructTypeInContext(c->context, params, 2, false);

  // descriptor, filled in
  gendesc_basetype(c, c->descriptor_type);

  // define object
  params[0] = c->descriptor_ptr;
  LLVMStructSetBody(c->object_type, params, 1, false);

  // $i8* pony_ctx()
  type = LLVMFunctionType(c->void_ptr, NULL, 0, false);
  value = LLVMAddFunction(c->module, "pony_ctx", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMAddFunctionAttr(value, LLVMReadNoneAttribute);

  // __object* pony_create(i8*, __Desc*)
  params[0] = c->void_ptr;
  params[1] = c->descriptor_ptr;
  type = LLVMFunctionType(c->object_ptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_create", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // void ponyint_destroy(__object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  value = LLVMAddFunction(c->module, "ponyint_destroy", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_sendv(i8*, __object*, $message*);
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  params[2] = c->msg_ptr;
  type = LLVMFunctionType(c->void_type, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_sendv", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // i8* pony_alloc(i8*, intptr)
  params[0] = c->void_ptr;
  params[1] = c->intptr;
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_alloc", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // i8* pony_alloc_small(i8*, i32)
  params[0] = c->void_ptr;
  params[1] = c->i32;
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_alloc_small", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // i8* pony_alloc_large(i8*, intptr)
  params[0] = c->void_ptr;
  params[1] = c->intptr;
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_alloc_large", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // i8* pony_realloc(i8*, i8*, intptr)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  params[2] = c->intptr;
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_realloc", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // i8* pony_alloc_final(i8*, intptr, c->final_fn)
  params[0] = c->void_ptr;
  params[1] = c->intptr;
  params[2] = c->final_fn;
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_alloc_final", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // $message* pony_alloc_msg(i32, i32)
  params[0] = c->i32;
  params[1] = c->i32;
  type = LLVMFunctionType(c->msg_ptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_alloc_msg", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
  LLVMSetReturnNoAlias(value);

  // void pony_trace(i8*, i8*)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_trace", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // i8* pony_traceobject(i8*, __object*, __Desc*, i32)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  params[2] = c->descriptor_ptr;
  params[3] = c->i32;
  type = LLVMFunctionType(c->void_ptr, params, 4, false);
  value = LLVMAddFunction(c->module, "pony_traceknown", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // i8* pony_traceunknown(i8*, __object*, i32)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  params[2] = c->i32;
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_traceunknown", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_gc_send(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  value = LLVMAddFunction(c->module, "pony_gc_send", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_gc_recv(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  value = LLVMAddFunction(c->module, "pony_gc_recv", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_send_done(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  value = LLVMAddFunction(c->module, "pony_send_done", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_recv_done(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  value = LLVMAddFunction(c->module, "pony_recv_done", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_serialise_reserve(i8*, i8*, intptr)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  params[2] = c->intptr;
  type = LLVMFunctionType(c->void_type, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_serialise_reserve", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // intptr pony_serialise_offset(i8*, i8*)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  type = LLVMFunctionType(c->intptr, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_serialise_offset", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // i8* pony_deserialise_offset(i8*, __desc*, intptr)
  params[0] = c->void_ptr;
  params[1] = c->descriptor_ptr;
  params[2] = c->intptr;
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_deserialise_offset", type);

  // i8* pony_deserialise_block(i8*, intptr, intptr)
  params[0] = c->void_ptr;
  params[1] = c->intptr;
  params[2] = c->intptr;
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  value = LLVMAddFunction(c->module, "pony_deserialise_block", type);

  // i32 pony_init(i32, i8**)
  params[0] = c->i32;
  params[1] = LLVMPointerType(c->void_ptr, 0);
  type = LLVMFunctionType(c->i32, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_init", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_become(i8*, __object*)
  params[0] = c->void_ptr;
  params[1] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  value = LLVMAddFunction(c->module, "pony_become", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // i32 pony_start(i32)
  params[0] = c->i32;
  type = LLVMFunctionType(c->i32, params, 1, false);
  value = LLVMAddFunction(c->module, "pony_start", type);
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void pony_throw()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_throw", type);

  // i32 pony_personality_v0(...)
  type = LLVMFunctionType(c->i32, NULL, 0, true);
  c->personality = LLVMAddFunction(c->module, "pony_personality_v0", type);

  // void llvm.memcpy.*(i8*, i8*, i32/64, i32, i1)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  params[3] = c->i32;
  params[4] = c->i1;
  if(target_is_ilp32(c->opt->triple))
  {
    params[2] = c->i32;
    type = LLVMFunctionType(c->void_type, params, 5, false);
    value = LLVMAddFunction(c->module, "llvm.memcpy.p0i8.p0i8.i32", type);
  } else {
    params[2] = c->i64;
    type = LLVMFunctionType(c->void_type, params, 5, false);
    value = LLVMAddFunction(c->module, "llvm.memcpy.p0i8.p0i8.i64", type);
  }
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);

  // void llvm.memmove.*(i8*, i8*, i32/64, i32, i1)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  params[3] = c->i32;
  params[4] = c->i1;
  if(target_is_ilp32(c->opt->triple))
  {
    params[2] = c->i32;
    type = LLVMFunctionType(c->void_type, params, 5, false);
    value = LLVMAddFunction(c->module, "llvm.memmove.p0i8.p0i8.i32", type);
  } else {
    params[2] = c->i64;
    type = LLVMFunctionType(c->void_type, params, 5, false);
    value = LLVMAddFunction(c->module, "llvm.memmove.p0i8.p0i8.i64", type);
  }
  LLVMAddFunctionAttr(value, LLVMNoUnwindAttribute);
}

static void init_module(compile_t* c, ast_t* program, pass_opt_t* opt)
{
  c->opt = opt;

  // Get the first package and the builtin package.
  ast_t* package = ast_child(program);
  ast_t* builtin = ast_sibling(package);

  // If we have only one package, we are compiling builtin itself.
  if(builtin == NULL)
    builtin = package;

  c->reach = reach_new();

  // The name of the first package is the name of the program.
  c->filename = package_filename(package);

  // LLVM context and machine settings.
  if(c->opt->library || target_is_ilp32(opt->triple))
    c->callconv = LLVMCCallConv;
  else
    c->callconv = LLVMFastCallConv;

  if(!c->opt->release || c->opt->library || c->opt->extfun)
    c->linkage = LLVMExternalLinkage;
  else
    c->linkage = LLVMPrivateLinkage;

  c->context = LLVMContextCreate();
  c->machine = make_machine(opt);
  c->target_data = LLVMGetTargetMachineData(c->machine);

  // Create a module.
  c->module = LLVMModuleCreateWithNameInContext(c->filename, c->context);

  // Set the target triple.
  LLVMSetTarget(c->module, opt->triple);

  // Set the data layout.
  char* layout = LLVMCopyStringRepOfTargetData(c->target_data);
  LLVMSetDataLayout(c->module, layout);
  LLVMDisposeMessage(layout);

  // IR builder.
  c->builder = LLVMCreateBuilderInContext(c->context);
  c->di = LLVMNewDIBuilder(c->module);

  // TODO: what LANG id should be used?
  c->di_unit = LLVMDIBuilderCreateCompileUnit(c->di, 0x0004,
    package_filename(package), package_path(package), "ponyc-" PONY_VERSION,
    c->opt->release);

  // Empty frame stack.
  c->frame = NULL;
}

static void codegen_cleanup(compile_t* c)
{
  while(c->frame != NULL)
    pop_frame(c);

  LLVMDIBuilderDestroy(c->di);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposeModule(c->module);
  LLVMContextDispose(c->context);
  LLVMDisposeTargetMachine(c->machine);
  reach_free(c->reach);
}

bool codegen_init(pass_opt_t* opt)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllAsmPrinters();
  LLVMInitializeAllAsmParsers();
  LLVMEnablePrettyStackTrace();
  LLVMInstallFatalErrorHandler(fatal_error);

  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeTransformUtils(passreg);
  LLVMInitializeScalarOpts(passreg);
  LLVMInitializeObjCARCOpts(passreg);
  LLVMInitializeVectorization(passreg);
  LLVMInitializeInstCombine(passreg);
  LLVMInitializeIPO(passreg);
  LLVMInitializeInstrumentation(passreg);
  LLVMInitializeAnalysis(passreg);
  LLVMInitializeIPA(passreg);
  LLVMInitializeCodeGen(passreg);
  LLVMInitializeTarget(passreg);

  // Default triple, cpu and features.
  if(opt->triple != NULL)
  {
    opt->triple = LLVMCreateMessage(opt->triple);
  } else {
#ifdef PLATFORM_IS_MACOSX
    // This is to prevent XCode 7+ from claiming OSX 14.5 is required.
    opt->triple = LLVMCreateMessage("x86_64-apple-macosx");
#else
    opt->triple = LLVMGetDefaultTargetTriple();
#endif
  }

  if(opt->cpu != NULL)
    opt->cpu = LLVMCreateMessage(opt->cpu);
  else
    opt->cpu = LLVMGetHostCPUName();

  if(opt->features != NULL)
    opt->features = LLVMCreateMessage(opt->features);
  else
    opt->features = LLVMCreateMessage("");

  return true;
}

void codegen_shutdown(pass_opt_t* opt)
{
  LLVMDisposeMessage(opt->triple);
  LLVMDisposeMessage(opt->cpu);
  LLVMDisposeMessage(opt->features);

  LLVMResetFatalErrorHandler();
  LLVMShutdown();
}

bool codegen(ast_t* program, pass_opt_t* opt)
{
  if(opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Generating\n");

  pony_mkdir(opt->output);

  compile_t c;
  memset(&c, 0, sizeof(compile_t));

  init_module(&c, program, opt);
  init_runtime(&c);
  genprim_reachable_init(&c, program);

  bool ok;

  if(c.opt->library)
    ok = genlib(&c, program);
  else
    ok = genexe(&c, program);

  codegen_cleanup(&c);
  return ok;
}

LLVMValueRef codegen_addfun(compile_t* c, const char* name, LLVMTypeRef type)
{
  // Add the function and set the calling convention and the linkage type.
  LLVMValueRef fun = LLVMAddFunction(c->module, name, type);
  LLVMSetFunctionCallConv(fun, c->callconv);
  LLVMSetLinkage(fun, c->linkage);

  LLVMValueRef arg = LLVMGetFirstParam(fun);
  uint32_t i = 1;

  while(arg != NULL)
  {
    LLVMTypeRef type = LLVMTypeOf(arg);

    if(LLVMGetTypeKind(type) == LLVMPointerTypeKind)
    {
      LLVMTypeRef elem = LLVMGetElementType(type);

      if(LLVMGetTypeKind(elem) == LLVMStructTypeKind)
      {
        size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem);
        LLVMSetDereferenceable(fun, i, size);
      }
    }

    arg = LLVMGetNextParam(arg);
    i++;
  }

  return fun;
}

void codegen_startfun(compile_t* c, LLVMValueRef fun, LLVMMetadataRef file,
  LLVMMetadataRef scope)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = fun;
  frame->is_function = true;
  frame->di_file = file;
  frame->di_scope = scope;

  if(LLVMCountBasicBlocks(fun) == 0)
  {
    LLVMBasicBlockRef block = codegen_block(c, "entry");
    LLVMPositionBuilderAtEnd(c->builder, block);
  }

  LLVMSetCurrentDebugLocation2(c->builder, 0, 0, NULL);
}

void codegen_finishfun(compile_t* c)
{
  pop_frame(c);
}

void codegen_pushscope(compile_t* c, ast_t* ast)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->break_target = frame->prev->break_target;
  frame->continue_target = frame->prev->continue_target;
  frame->invoke_target = frame->prev->invoke_target;
  frame->di_file = frame->prev->di_file;

  if(frame->prev->di_scope != NULL)
  {
    frame->di_scope = LLVMDIBuilderCreateLexicalBlock(c->di,
      frame->prev->di_scope, frame->di_file,
      (unsigned)ast_line(ast), (unsigned)ast_pos(ast));
  }
}

void codegen_popscope(compile_t* c)
{
  pop_frame(c);
}

LLVMMetadataRef codegen_difile(compile_t* c)
{
  return c->frame->di_file;
}

LLVMMetadataRef codegen_discope(compile_t* c)
{
  return c->frame->di_scope;
}

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->break_target = break_target;
  frame->continue_target = continue_target;
  frame->invoke_target = frame->prev->invoke_target;
  frame->di_file = frame->prev->di_file;
  frame->di_scope = frame->prev->di_scope;
}

void codegen_poploop(compile_t* c)
{
  pop_frame(c);
}

void codegen_pushtry(compile_t* c, LLVMBasicBlockRef invoke_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->break_target = frame->prev->break_target;
  frame->continue_target = frame->prev->continue_target;
  frame->invoke_target = invoke_target;
  frame->di_file = frame->prev->di_file;
  frame->di_scope = frame->prev->di_scope;
}

void codegen_poptry(compile_t* c)
{
  pop_frame(c);
}

void codegen_debugloc(compile_t* c, ast_t* ast)
{
  if(ast != NULL)
  {
    LLVMSetCurrentDebugLocation2(c->builder,
      (unsigned)ast_line(ast), (unsigned)ast_pos(ast), c->frame->di_scope);
  } else {
    LLVMSetCurrentDebugLocation2(c->builder, 0, 0, NULL);
  }
}

LLVMValueRef codegen_getlocal(compile_t* c, const char* name)
{
  compile_frame_t* frame = c->frame;

  compile_local_t k;
  k.name = name;

  while(frame != NULL)
  {
    compile_local_t* p = compile_locals_get(&frame->locals, &k);

    if(p != NULL)
      return p->alloca;

    if(frame->is_function)
      return NULL;

    frame = frame->prev;
  }

  return NULL;
}

void codegen_setlocal(compile_t* c, const char* name, LLVMValueRef alloca)
{
  compile_local_t* p = POOL_ALLOC(compile_local_t);
  p->name = name;
  p->alloca = alloca;

  compile_locals_put(&c->frame->locals, p);
}

LLVMValueRef codegen_ctx(compile_t* c)
{
  compile_frame_t* frame = c->frame;

  while(!frame->is_function)
    frame = frame->prev;

  if(frame->ctx == NULL)
  {
    LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(frame->fun);
    LLVMValueRef inst = LLVMGetFirstInstruction(entry_block);

    if(inst != NULL)
      LLVMPositionBuilderBefore(c->builder, inst);
    else
      LLVMPositionBuilderAtEnd(c->builder, entry_block);

    frame->ctx = gencall_runtime(c, "pony_ctx", NULL, 0, "");
    LLVMPositionBuilderAtEnd(c->builder, this_block);
  }

  return frame->ctx;
}

void codegen_setctx(compile_t* c, LLVMValueRef ctx)
{
  compile_frame_t* frame = c->frame;

  while(!frame->is_function)
    frame = frame->prev;

  frame->ctx = ctx;
}

LLVMValueRef codegen_fun(compile_t* c)
{
  return c->frame->fun;
}

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name)
{
  return LLVMAppendBasicBlockInContext(c->context, c->frame->fun, name);
}

LLVMValueRef codegen_call(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  size_t count)
{
  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, (int)count, "");
  LLVMSetInstructionCallConv(result, c->callconv);
  return result;
}

const char* suffix_filename(compile_t* c, const char* dir, const char* prefix,
  const char* file, const char* extension)
{
  // Copy to a string with space for a suffix.
  size_t len = strlen(dir) + strlen(prefix) + strlen(file) + strlen(extension)
    + 4;
  char* filename = (char*)ponyint_pool_alloc_size(len);

  // Start with no suffix.
#ifdef PLATFORM_IS_WINDOWS
  snprintf(filename, len, "%s\\%s%s%s", dir, prefix, file, extension);
#else
  snprintf(filename, len, "%s/%s%s%s", dir, prefix, file, extension);
#endif

  int suffix = 0;

  while(suffix < 100)
  {
    // Overwrite files but not directories.
    struct stat s;
    int err = stat(filename, &s);

    if((err == -1) || !S_ISDIR(s.st_mode))
      break;

#ifdef PLATFORM_IS_WINDOWS
    snprintf(filename, len, "%s\\%s%s%d%s", dir, prefix, file, ++suffix,
      extension);
#else
    snprintf(filename, len, "%s/%s%s%d%s", dir, prefix, file, ++suffix,
      extension);
#endif
  }

  if(suffix >= 100)
  {
    errorf(c->opt->check.errors, NULL, "couldn't pick an unused file name");
    ponyint_pool_free_size(len, filename);
    return NULL;
  }

  return stringtab_consume(filename, len);
}
