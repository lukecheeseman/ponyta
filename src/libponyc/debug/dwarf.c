#include "dwarf.h"
#include "symbols.h"
#include "../type/subtype.h"
#include "../codegen/gentype.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/pony.h"

#include <string.h>
#include <assert.h>

static void setup_dwarf(dwarf_t* dwarf, dwarf_meta_t* meta, gentype_t* g,
  bool opaque, bool field)
{
  memset(meta, 0, sizeof(dwarf_meta_t));

  ast_t* ast = g->ast;
  LLVMTypeRef type = g->primitive;

  if(is_machine_word(ast))
  {
    if(is_float(ast))
      meta->flags |= DWARF_FLOAT;
    else if(is_signed(dwarf->opt, ast))
      meta->flags |= DWARF_SIGNED;
    else if(is_bool(ast))
      meta->flags |= DWARF_BOOLEAN;
  }
  else if(is_pointer(ast) || is_maybe(ast) || !is_concrete(ast) ||
    (is_constructable(ast) && field))
  {
    type = g->use_type;
  }
  else if(is_constructable(ast))
  {
    type = g->structure;
  }

  bool defined_type = g->underlying != TK_TUPLETYPE &&
    g->underlying != TK_UNIONTYPE && g->underlying != TK_ISECTTYPE;

  source_t* source;

  if(defined_type)
    ast = (ast_t*)ast_data(ast);

  source = ast_source(ast);
  meta->file = source->file;
  meta->name = g->type_name;
  meta->line = ast_line(ast);
  meta->pos = ast_pos(ast);

  if(!opaque)
  {
    meta->size = (size_t)(8 * LLVMABISizeOfType(dwarf->target_data, type));
    meta->align = 8 * LLVMABIAlignmentOfType(dwarf->target_data, type);
  }
}

static void meta_local(dwarf_meta_t* meta, ast_t* ast, const char* name,
  const char* type, LLVMBasicBlockRef entry, LLVMValueRef storage,
  size_t offset, bool constant)
{
  memset(meta, 0, sizeof(dwarf_meta_t));

  source_t* source = ast_source(ast);

  meta->file = source->file;
  meta->name = name;
  meta->mangled = type;
  meta->line = ast_line(ast);
  meta->pos = ast_pos(ast);
  meta->offset = offset + 1;
  meta->entry = entry;
  meta->storage = storage;

  if(constant)
    meta->flags = DWARF_CONSTANT;
}

void dwarf_compileunit(dwarf_t* dwarf, ast_t* program)
{
  assert(ast_id(program) == TK_PROGRAM);
  ast_t* package = ast_child(program);

  const char* path = package_path(package);
  const char* name = package_filename(package); //FIX

  symbols_package(dwarf->symbols, path, name);
}

void dwarf_basic(dwarf_t* dwarf, gentype_t* g)
{
  symbols_push_frame(dwarf->symbols, g);

  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, g, false, false);

  symbols_basic(dwarf->symbols, &meta);
}

void dwarf_pointer(dwarf_t* dwarf, gentype_t* g, const char* typearg)
{
  symbols_push_frame(dwarf->symbols, g);

  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, g, false, false);

  meta.typearg = typearg;
  symbols_pointer(dwarf->symbols, &meta);
}

void dwarf_trait(dwarf_t* dwarf, gentype_t* g)
{
  // Trait definitions have a scope, but are modeled
  // as opaque classes from which other classes may
  // inherit.
  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, g, false, false);

  symbols_trait(dwarf->symbols, &meta);
}

void dwarf_forward(dwarf_t* dwarf, gentype_t* g)
{
  symbols_push_frame(dwarf->symbols, g);

  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, g, true, false);

  symbols_declare(dwarf->symbols, &meta);
}

void dwarf_composite(dwarf_t* dwarf, gentype_t* g)
{
  if(is_machine_word(g->ast) || is_pointer(g->ast))
    return;

  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, g, false, false);

  symbols_composite(dwarf->symbols, &meta);
}

void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field,
  int index)
{
  char buf[32];
  memset(buf, 0, sizeof(buf));

  dwarf_meta_t meta;
  setup_dwarf(dwarf, &meta, field, false, true);

  meta.typearg = field->type_name;

  if(composite->underlying == TK_TUPLETYPE)
  {
    meta.flags |= DWARF_CONSTANT;
    snprintf(buf, sizeof(buf), "_%d", index + 1);
    meta.name = buf;
  }
  else
  {
    ast_t* def = (ast_t*)ast_data(composite->ast);
    ast_t* members = ast_childidx(def, 4);
    ast_t* fld = ast_childidx(members, index);
    meta.name = ast_name(ast_child(fld));

    if(ast_id(fld) == TK_FLET)
      meta.flags |= DWARF_CONSTANT;
  }

  if(meta.name[0] == '_')
    meta.flags |= DWARF_PRIVATE;

  LLVMTypeRef structure = composite->primitive;
  int offset = 0;

  if(composite->underlying != TK_TUPLETYPE)
  {
    structure = composite->structure;

    if(composite->underlying != TK_STRUCT)
      offset++;

    if(composite->underlying == TK_ACTOR)
      offset++;
  }

  meta.offset = (size_t)(8 * LLVMOffsetOfElement(dwarf->target_data, structure,
    offset + index));

  symbols_field(dwarf->symbols, &meta);
}

void dwarf_method(dwarf_t* dwarf, ast_t* fun, const char* name,
  const char* mangled, const char** params, size_t count, LLVMValueRef ir)
{
  dwarf_meta_t meta;
  memset(&meta, 0, sizeof(dwarf_meta_t));

  source_t* source = ast_source(fun);
  ast_t* seq = ast_childidx(fun, 6);

  meta.file = source->file;
  meta.name = name;
  meta.mangled = mangled;
  meta.params = params;
  meta.line = ast_line(fun);
  meta.pos = ast_pos(fun);
  meta.offset = ast_line(seq);
  meta.size = count;

  symbols_method(dwarf->symbols, &meta, ir);
}

void dwarf_lexicalscope(dwarf_t* dwarf, ast_t* ast)
{
  dwarf_meta_t meta;
  memset(&meta, 0, sizeof(dwarf_meta_t));

  symbols_push_frame(dwarf->symbols, NULL);
  source_t* source = ast_source(ast);

  meta.file = source->file;
  meta.line = ast_line(ast);
  meta.pos = ast_pos(ast);

  symbols_lexicalscope(dwarf->symbols, &meta);
}

void dwarf_this(dwarf_t* dwarf, ast_t* fun, const char* type,
  LLVMBasicBlockRef entry, LLVMValueRef storage)
{
  dwarf_meta_t meta;
  meta_local(&meta, fun, stringtab("this"), type, entry, storage, 0, true);
  meta.flags |= DWARF_ARTIFICIAL;

  symbols_local(dwarf->symbols, &meta, true);
}

void dwarf_parameter(dwarf_t* dwarf, ast_t* param, const char* type,
  LLVMBasicBlockRef entry, LLVMValueRef storage, size_t index)
{
  const char* name = ast_name(ast_child(param));

  dwarf_meta_t meta;
  meta_local(&meta, param, name, type, entry, storage, index, true);

  symbols_local(dwarf->symbols, &meta, true);
}

void dwarf_local(dwarf_t* dwarf, ast_t* ast, const char* type,
  LLVMBasicBlockRef entry, LLVMValueRef inst, LLVMValueRef storage)
{
  const char* name = ast_name(ast_child(ast));

  dwarf_meta_t meta;
  meta_local(&meta, ast, name, type, entry, storage, 0, ast_id(ast) != TK_VAR);

  meta.inst = inst;

  symbols_local(dwarf->symbols, &meta, false);
}

void dwarf_location(dwarf_t* dwarf, ast_t* ast)
{
  if(dwarf->has_source && (ast != NULL))
  {
    size_t line = ast_line(ast);
    size_t pos = ast_pos(ast);

    if(!ast_debug(ast))
      symbols_reset(dwarf->symbols, true);
    else
      symbols_location(dwarf->symbols, line, pos);
  }
  else if(dwarf->symbols != NULL)
  {
    symbols_reset(dwarf->symbols, true);
  }
}

void dwarf_finish(dwarf_t* dwarf)
{
  symbols_pop_frame(dwarf->symbols);
  symbols_reset(dwarf->symbols, false);
}

void dwarf_init(dwarf_t* dwarf, pass_opt_t* opt, LLVMBuilderRef builder,
  LLVMTargetDataRef layout, LLVMModuleRef module)
{
  dwarf->opt = opt;
  dwarf->target_data = layout;
  dwarf->has_source = false;

  symbols_init(&dwarf->symbols, builder, module, opt->release);
  symbols_unspecified(dwarf->symbols, stringtab("$object"));
}

void dwarf_finalise(dwarf_t* dwarf)
{
  symbols_finalise(dwarf->symbols);
}
