#include "package.h"
#include "program.h"
#include "use.h"
#include "../codegen/codegen.h"
#include "../ast/source.h"
#include "../ast/parser.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#if defined(PLATFORM_IS_LINUX)
#include <unistd.h>
#elif defined(PLATFORM_IS_MACOSX)
#include <mach-o/dyld.h>
#elif defined(PLATFORM_IS_FREEBSD)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif


#define EXTENSION ".pony"


#ifdef PLATFORM_IS_WINDOWS
# define PATH_SLASH '\\'
# define PATH_LIST_SEPARATOR ';'
#else
# define PATH_SLASH '/'
# define PATH_LIST_SEPARATOR ':'
#endif


#ifdef PLATFORM_IS_VISUAL_STUDIO
/** Disable warning about "getenv" begin unsafe. The alternatives, s_getenv and
 *  _dupenv_s are incredibly inconvenient and expensive to use. Since such a
 *  warning could make sense for other function calls, we only disable it for
 *  this file.
 */
#  pragma warning(disable:4996)
#endif

// Per package state
typedef struct package_t
{
  const char* path; // Absolute path
  const char* qualified_name; // For pretty printing, eg "builtin/U32"
  const char* id; // Hygienic identifier
  const char* filename; // Filename if we are an executable
  const char* symbol; // Wart to use for symbol names
  size_t next_hygienic_id;
  bool allow_ffi;
} package_t;

// Per defined magic package sate
typedef struct magic_package_t
{
  const char* path;
  const char* src;
  struct magic_package_t* next;
} magic_package_t;

// Function that will handle a path in some way.
typedef bool (*path_fn)(const char* path);

// Global state
static strlist_t* search = NULL;
static strlist_t* safe = NULL;
static magic_package_t* magic_packages = NULL;
static bool report_build = true;


// Find the magic source code associated with the given path, if any
static const char* find_magic_package(const char* path)
{
  for(magic_package_t* p = magic_packages; p != NULL; p = p->next)
  {
    if(path == p->path)
      return p->src;
  }

  return NULL;
}


// Attempt to parse the specified source file and add it to the given AST
// @return true on success, false on error
static bool parse_source_file(ast_t* package, const char* file_path,
  pass_opt_t* opt)
{
  assert(package != NULL);
  assert(file_path != NULL);
  assert(opt != NULL);

  if(opt->print_filenames)
    printf("Opening %s\n", file_path);

  const char* error_msg = NULL;
  source_t* source = source_open(file_path, &error_msg);

  if(source == NULL)
  {
    if(error_msg == NULL)
      error_msg = "couldn't open file";

    errorf(opt->check.errors, file_path, "%s %s", error_msg, file_path);
    return false;
  }

  return module_passes(package, opt, source);
}


// Attempt to parse the specified source code and add it to the given AST
// @return true on success, false on error
static bool parse_source_code(ast_t* package, const char* src,
  pass_opt_t* opt)
{
  assert(src != NULL);
  assert(opt != NULL);

  if(opt->print_filenames)
    printf("Opening magic source\n");

  source_t* source = source_open_string(src);
  assert(source != NULL);

  return module_passes(package, opt, source);
}


// Cat together the 2 given path fragments into the given buffer.
// The first path fragment may be absolute, relative or NULL
static void path_cat(const char* part1, const char* part2,
  char result[FILENAME_MAX])
{
  size_t len1 = 0;
  size_t lensep = 0;

  if(part1 != NULL)
  {
    len1 = strlen(part1);
    lensep = 1;
  }

  size_t len2 = strlen(part2);

  if((len1 + lensep + len2) >= FILENAME_MAX)
  {
    result[0] = '\0';
    return;
  }

  if(part1 != NULL)
  {
    memcpy(result, part1, len1);
    result[len1] = PATH_SLASH;
    memcpy(&result[len1 + 1], part2, len2 + 1);
  } else {
    memcpy(result, part2, len2 + 1);
  }
}


// Attempt to parse the source files in the specified directory and add them to
// the given package AST
// @return true on success, false on error
static bool parse_files_in_dir(ast_t* package, const char* dir_path,
  pass_opt_t* opt)
{
  PONY_ERRNO err = 0;
  PONY_DIR* dir = pony_opendir(dir_path, &err);
  errors_t* errors = opt->check.errors;

  if(dir == NULL)
  {
    switch(err)
    {
      case EACCES:  errorf(errors, dir_path, "permission denied"); break;
      case ENOENT:  errorf(errors, dir_path, "does not exist");    break;
      case ENOTDIR: errorf(errors, dir_path, "not a directory");   break;
      default:      errorf(errors, dir_path, "unknown error");     break;
    }

    return false;
  }

  PONY_DIRINFO* d;
  bool r = true;

  while((d = pony_dir_entry_next(dir)) != NULL)
  {
    // Handle only files with the specified extension that don't begin with
    // a dot. This avoids including UNIX hidden files in a build.
    char* name = pony_dir_info_name(d);

    if(name[0] == '.')
      continue;

    const char* p = strrchr(name, '.');

    if((p != NULL) && (strcmp(p, EXTENSION) == 0))
    {
      char fullpath[FILENAME_MAX];
      path_cat(dir_path, name, fullpath);
      r &= parse_source_file(package, fullpath, opt);
    }
  }

  pony_closedir(dir);
  return r;
}


// Check whether the directory specified by catting the given base and path
// exists
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* try_path(const char* base, const char* path)
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  path_cat(base, path, composite);

  if(pony_realpath(composite, file) != file)
    return NULL;

  struct stat s;
  int err = stat(file, &s);

  if((err != -1) && S_ISDIR(s.st_mode))
    return stringtab(file);

  return NULL;
}


static bool is_root(const char* path)
{
  assert(path != NULL);

#if defined(PLATFORM_IS_WINDOWS)
  assert(path[0] != '\0');
  assert(path[1] == ':');

  if((path[2] == '\0'))
    return true;

  if((path[2] == '\\') && (path[3] == '\0'))
    return true;
#else
  if((path[0] == '/') && (path[1] == '\0'))
    return true;
#endif

  return false;
}


// Try base/../pony_packages/path, and keep adding .. to look another level up
// until we are looking in /pony_packages/path
static const char* try_package_path(const char* base, const char* path)
{
  char path1[FILENAME_MAX];
  char path2[FILENAME_MAX];
  path_cat(NULL, base, path1);

  do
  {
    path_cat(path1, "..", path2);

    if(pony_realpath(path2, path1) != path1)
      break;

    path_cat(path1, "pony_packages", path2);

    const char* result = try_path(path2, path);

    if(result != NULL)
      return result;
  } while(!is_root(path1));

  return NULL;
}


// Attempt to find the specified package directory in our search path
// @return The resulting directory path, which should not be deleted and is
// valid indefinitely. NULL is directory cannot be found.
static const char* find_path(ast_t* from, const char* path,
  bool* out_is_relative)
{
  if(out_is_relative != NULL)
    *out_is_relative = false;

  // First check for an absolute path
  if(is_path_absolute(path))
    return try_path(NULL, path);

  // Get the base directory
  const char* base;

  if((from == NULL) || (ast_id(from) == TK_PROGRAM))
  {
    base = NULL;
  } else {
    from = ast_nearest(from, TK_PACKAGE);
    package_t* pkg = (package_t*)ast_data(from);
    base = pkg->path;
  }

  // Try a path relative to the base
  const char* result = try_path(base, path);

  if(result != NULL)
  {
    if(out_is_relative != NULL)
      *out_is_relative = true;

    return result;
  }

  // If it's a relative path, don't try elsewhere
  if(!is_path_relative(path))
  {
    // Check ../pony_packages and further up the tree
    if(base != NULL)
    {
      result = try_package_path(base, path);

      if(result != NULL)
        return result;

      // Check ../pony_packages from the compiler target
      if((from != NULL) && (ast_id(from) == TK_PACKAGE))
      {
        ast_t* target = ast_child(ast_parent(from));
        package_t* pkg = (package_t*)ast_data(target);
        base = pkg->path;

        result = try_package_path(base, path);

        if(result != NULL)
          return result;
      }
    }

    // Try the search paths
    for(strlist_t* p = search; p != NULL; p = strlist_next(p))
    {
      result = try_path(strlist_data(p), path);

      if(result != NULL)
        return result;
    }
  }

  return NULL;
}


// Convert the given ID to a hygenic string. The resulting string should not be
// deleted and is valid indefinitely.
static const char* id_to_string(const char* prefix, size_t id)
{
  if(prefix == NULL)
    prefix = "";

  size_t len = strlen(prefix);
  size_t buf_size = len + 32;
  char* buffer = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buffer, buf_size, "%s$" __zu, prefix, id);
  return stringtab_consume(buffer, buf_size);
}


static bool symbol_in_use(ast_t* program, const char* symbol)
{
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    package_t* pkg = (package_t*)ast_data(package);

    if(pkg->symbol == symbol)
      return true;

    package = ast_sibling(package);
  }

  return false;
}


static const char* string_to_symbol(const char* string)
{
  bool prefix = false;

  if(!((string[0] >= 'a') && (string[0] <= 'z')) &&
    !((string[0] >= 'A') && (string[0] <= 'Z')))
  {
    // If it doesn't start with a letter, prefix an underscore.
    prefix = true;
  }

  size_t len = strlen(string);
  size_t buf_size = len + prefix + 1;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  memcpy(buf + prefix, string, len + 1);

  if(prefix)
    buf[0] = '_';

  for(size_t i = prefix; i < len; i++)
  {
    if(
      (buf[i] == '_') ||
      ((buf[i] >= 'a') && (buf[i] <= 'z')) ||
      ((buf[i] >= '0') && (buf[i] <= '9'))
      )
    {
      // Do nothing.
    } else if((buf[i] >= 'A') && (buf[i] <= 'Z')) {
      // Force lower case.
      buf[i] |= 0x20;
    } else {
      // Smash a non-symbol character to an underscore.
      buf[i] = '_';
    }
  }

  return stringtab_consume(buf, buf_size);
}


static const char* symbol_suffix(const char* symbol, size_t suffix)
{
  size_t len = strlen(symbol);
  size_t buf_size = len + 32;
  char* buf = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buf, buf_size, "%s" __zu, symbol, suffix);

  return stringtab_consume(buf, buf_size);
}


static const char* create_package_symbol(ast_t* program, const char* filename)
{
  const char* symbol = string_to_symbol(filename);
  size_t suffix = 1;

  while(symbol_in_use(program, symbol))
  {
    symbol = symbol_suffix(symbol, suffix);
    suffix++;
  }

  return symbol;
}


// Create a package AST, set up its state and add it to the given program
static ast_t* create_package(ast_t* program, const char* name,
  const char* qualified_name)
{
  ast_t* package = ast_blank(TK_PACKAGE);
  uint32_t pkg_id = program_assign_pkg_id(program);

  package_t* pkg = POOL_ALLOC(package_t);
  pkg->path = name;
  pkg->qualified_name = qualified_name;
  pkg->id = id_to_string(NULL, pkg_id);

  const char* p = strrchr(pkg->path, PATH_SLASH);

  if(p == NULL)
    p = pkg->path;
  else
    p = p + 1;

  pkg->filename = stringtab(p);

  if(pkg_id > 1)
    pkg->symbol = create_package_symbol(program, pkg->filename);
  else
    pkg->symbol = NULL;

  pkg->next_hygienic_id = 0;
  ast_setdata(package, pkg);

  ast_scope(package);
  ast_append(program, package);
  ast_set(program, pkg->path, package, SYM_NONE, false);
  ast_set(program, pkg->id, package, SYM_NONE, false);

  if((safe != NULL) && (strlist_find(safe, pkg->path) == NULL))
    pkg->allow_ffi = false;
  else
    pkg->allow_ffi = true;

  return package;
}


// Check that the given path exists and add it to our package search paths
static bool add_path(const char* path)
{
#ifdef PLATFORM_IS_WINDOWS
  // The Windows implementation of stat() cannot cope with trailing a \ on a
  // directory name, so we remove it here if present. It is not safe to modify
  // the given path since it may come direct from getenv(), so we copy.
  char buf[FILENAME_MAX];
  strcpy(buf, path);
  size_t len = strlen(path);

  if(path[len - 1] == '\\')
  {
    buf[len - 1] = '\0';
    path = buf;
  }
#endif

  struct stat s;
  int err = stat(path, &s);

  if((err != -1) && S_ISDIR(s.st_mode))
  {
    path = stringtab(path);

    if(strlist_find(search, path) == NULL)
      search = strlist_append(search, path);
  }

  return true;
}


static bool add_safe(const char* path)
{
  path = stringtab(path);

  if(strlist_find(safe, path) == NULL)
    safe = strlist_append(safe, path);

  return true;
}


// Determine the absolute path of the directory the current executable is in
// and add it to our search path
static void add_exec_dir(pass_opt_t* opt)
{
  char path[FILENAME_MAX];
  bool success;
  errors_t* errors = opt->check.errors;

#ifdef PLATFORM_IS_WINDOWS
  // Specified size *includes* nul terminator
  GetModuleFileName(NULL, path, FILENAME_MAX);
  success = (GetLastError() == ERROR_SUCCESS);
#elif defined PLATFORM_IS_LINUX
  // Specified size *excludes* nul terminator
  ssize_t r = readlink("/proc/self/exe", path, FILENAME_MAX - 1);
  success = (r >= 0);

  if(success)
    path[r] = '\0';
#elif defined PLATFORM_IS_FREEBSD
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  size_t len = FILENAME_MAX;
  int r = sysctl(mib, 4, path, &len, NULL, 0);
  success = (r == 0);
#elif defined PLATFORM_IS_MACOSX
  char exec_path[FILENAME_MAX];
  uint32_t size = sizeof(exec_path);
  int r = _NSGetExecutablePath(exec_path, &size);
  success = (r == 0);

  if(success)
  {
    pony_realpath(exec_path, path);
  }
#else
#  error Unsupported platform for exec_path()
#endif

  if(!success)
  {
    errorf(errors, NULL, "Error determining executable path");
    return;
  }

  // We only need the directory
  char *p = strrchr(path, PATH_SLASH);

  if(p == NULL)
  {
    errorf(errors, NULL, "Error determining executable path (%s)", path);
    return;
  }

  p++;
  *p = '\0';
  add_path(path);

  // Allow ponyc to find the lib directory when it is installed.
#ifdef PLATFORM_IS_WINDOWS
  strcpy(p, "..\\lib");
#else
  strcpy(p, "../lib");
#endif
  add_path(path);

  // Allow ponyc to find the packages directory when it is installed.
#ifdef PLATFORM_IS_WINDOWS
  strcpy(p, "..\\packages");
#else
  strcpy(p, "../packages");
#endif
  add_path(path);

  // Check two levels up. This allows ponyc to find the packages directory
  // when it is built from source.
#ifdef PLATFORM_IS_WINDOWS
  strcpy(p, "..\\..\\packages");
#else
  strcpy(p, "../../packages");
#endif
  add_path(path);
}


bool package_init(pass_opt_t* opt)
{
  // package_add_paths for command line paths has already been done. Here, we
  // append the paths from an optional environment variable, and then the paths
  // that are relative to the compiler location on disk.
  package_add_paths(getenv("PONYPATH"), opt);
  add_exec_dir(opt);

  // Finally we add OS specific paths.
#ifdef PLATFORM_IS_POSIX_BASED
  add_path("/usr/local/lib");
  add_path("/opt/local/lib");
#endif

  // Convert all the safe packages to their full paths.
  strlist_t* full_safe = NULL;

  while(safe != NULL)
  {
    const char* path;
    safe = strlist_pop(safe, &path);

    // Lookup (and hence normalise) path.
    path = find_path(NULL, path, NULL);

    if(path == NULL)
    {
      strlist_free(full_safe);
      return false;
    }

    full_safe = strlist_push(full_safe, path);
  }

  safe = full_safe;
  return true;
}


strlist_t* package_paths()
{
  return search;
}


static bool handle_path_list(const char* paths, path_fn f, pass_opt_t* opt)
{
  if(paths == NULL)
    return true;

  bool ok = true;

  while(true)
  {
    // Find end of next path
    const char* p = strchr(paths, PATH_LIST_SEPARATOR);
    size_t len;

    if(p != NULL)
    {
      // Separator found
      len = p - paths;
    } else {
      // Separator not found, this is the last path in the list
      len = strlen(paths);
    }

    if(len >= FILENAME_MAX)
    {
      errorf(opt->check.errors, NULL, "Path too long in %s", paths);
    } else {
      char path[FILENAME_MAX];

      strncpy(path, paths, len);
      path[len] = '\0';
      ok = f(path) && ok;
    }

    if(p == NULL) // No more separators
      break;

    paths = p + 1;
  }

  return ok;
}

void package_add_paths(const char* paths, pass_opt_t* opt)
{
  handle_path_list(paths, add_path, opt);
}


bool package_add_safe(const char* paths, pass_opt_t* opt)
{
  add_safe("builtin");
  return handle_path_list(paths, add_safe, opt);
}


void package_add_magic(const char* path, const char* src)
{
  magic_package_t* n = POOL_ALLOC(magic_package_t);
  n->path = stringtab(path);
  n->src = src;
  n->next = magic_packages;
  magic_packages = n;
}


void package_clear_magic()
{
  magic_package_t*p = magic_packages;

  while(p != NULL)
  {
    magic_package_t* next = p->next;
    POOL_FREE(magic_package_t, p);
    p = next;
  }

  magic_packages = NULL;
}


void package_suppress_build_message()
{
  report_build = false;
}


ast_t* program_load(const char* path, pass_opt_t* opt)
{
  ast_t* program = ast_blank(TK_PROGRAM);
  ast_scope(program);

  opt->program_pass = PASS_PARSE;

  // Always load builtin package first, then the specified one.
  if(package_load(program, stringtab("builtin"), opt) == NULL ||
    package_load(program, path, opt) == NULL)
  {
    ast_free(program);
    return NULL;
  }

  // Reorder packages so specified package is first.
  ast_t* builtin = ast_pop(program);
  ast_append(program, builtin);

  if(!ast_passes_program(program, opt))
  {
    ast_free(program);
    return NULL;
  }

  return program;
}


ast_t* package_load(ast_t* from, const char* path, pass_opt_t* opt)
{
  assert(from != NULL);

  const char* magic = find_magic_package(path);
  const char* full_path = path;
  const char* qualified_name = path;
  ast_t* program = ast_nearest(from, TK_PROGRAM);

  if(magic == NULL)
  {
    // Lookup (and hence normalise) path
    bool is_relative = false;
    full_path = find_path(from, path, &is_relative);

    if(full_path == NULL)
    {
      errorf(opt->check.errors, path, "couldn't locate this path");
      return NULL;
    }

    if((from != program) && is_relative)
    {
      // Package to load is relative to from, build the qualified name
      // The qualified name should be relative to the program being built
      package_t* from_pkg = (package_t*)ast_data(ast_child(program));

      if(from_pkg != NULL)
      {
        const char* base_name = from_pkg->qualified_name;
        size_t base_name_len = strlen(base_name);
        size_t path_len = strlen(path);
        size_t len = base_name_len + path_len + 2;
        char* q_name = (char*)ponyint_pool_alloc_size(len);
        memcpy(q_name, base_name, base_name_len);
        q_name[base_name_len] = '/';
        memcpy(q_name + base_name_len + 1, path, path_len);
        q_name[len - 1] = '\0';
        qualified_name = stringtab_consume(q_name, len);
      }
    }
  }

  ast_t* package = ast_get(program, full_path, NULL);

  // Package already loaded
  if(package != NULL)
    return package;

  package = create_package(program, full_path, qualified_name);

  if(report_build) {
    PONY_LOG(opt, VERBOSITY_INFO, ("Building %s -> %s\n", path, full_path));
  }

  if(magic != NULL)
  {
    if(!parse_source_code(package, magic, opt))
      return NULL;
  }
  else
  {
    if(!parse_files_in_dir(package, full_path, opt))
      return NULL;
  }

  if(ast_child(package) == NULL)
  {
    ast_error(opt->check.errors, package,
      "no source files in package '%s'", path);
    return NULL;
  }

  if(!ast_passes_subtree(&package, opt, opt->program_pass))
  {
    // If these passes failed, don't run future passes.
    ast_setflag(package, AST_FLAG_PRESERVE);
    return NULL;
  }

  return package;
}


void package_free(package_t* package)
{
  if(package != NULL)
    POOL_FREE(package_t, package);
}


const char* package_name(ast_t* ast)
{
  package_t* pkg = (package_t*)ast_data(ast_nearest(ast, TK_PACKAGE));
  return (pkg == NULL) ? NULL : pkg->id;
}


ast_t* package_id(ast_t* ast)
{
  return ast_from_string(ast, package_name(ast));
}


const char* package_path(ast_t* package)
{
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);

  return pkg->path;
}


const char* package_qualified_name(ast_t* package)
{
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);

  return pkg->qualified_name;
}


const char* package_filename(ast_t* package)
{
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  assert(pkg != NULL);

  return pkg->filename;
}


const char* package_symbol(ast_t* package)
{
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);
  package_t* pkg = (package_t*)ast_data(package);
  assert(pkg != NULL);

  return pkg->symbol;
}


const char* package_hygienic_id(typecheck_t* t)
{
  assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  size_t id = pkg->next_hygienic_id++;

  return id_to_string(pkg->id, id);
}


bool package_allow_ffi(typecheck_t* t)
{
  assert(t->frame->package != NULL);
  package_t* pkg = (package_t*)ast_data(t->frame->package);
  return pkg->allow_ffi;
}


void package_done()
{
  strlist_free(search);
  search = NULL;

  strlist_free(safe);
  safe = NULL;

  package_clear_magic();
}


bool is_path_absolute(const char* path)
{
  // Begins with /
  if(path[0] == '/')
    return true;

#if defined(PLATFORM_IS_WINDOWS)
  // Begins with \ or ?:
  if(path[0] == '\\')
    return true;

  if((path[0] != '\0') && (path[1] == ':'))
    return true;
#endif

  return false;
}


bool is_path_relative(const char* path)
{
  if(path[0] == '.')
  {
    // Begins with ./
    if(path[1] == '/')
      return true;

#if defined(PLATFORM_IS_WINDOWS)
    // Begins with .\ on windows
    if(path[1] == '\\')
      return true;
#endif

    if(path[1] == '.')
    {
      // Begins with ../
      if(path[2] == '/')
        return true;

#if defined(PLATFORM_IS_WINDOWS)
      // Begins with ..\ on windows
      if(path[2] == '\\')
        return true;
#endif
    }
  }

  return false;
}
