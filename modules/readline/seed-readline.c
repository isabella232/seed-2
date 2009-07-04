#include <seed-module.h>

#include <stdio.h>
#include <ffi.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/mman.h>

SeedObject namespace_ref;
SeedEngine *eng;
gboolean readline_has_initialized = FALSE;

static void
seed_handle_rl_closure(ffi_cif * cif, void *result, void **args, void *userdata)
{
  SeedContext ctx = seed_context_create(eng->group, NULL);
  SeedValue exception = 0;
  SeedObject function = (SeedObject) userdata;

  seed_object_call(ctx, function, 0, 0, 0, &exception);
  if (exception)
    {
      gchar *mes = seed_exception_to_string(ctx,
					    exception);
      g_warning("Exception in readline bind key closure. %s \n", mes, 0);
    }
  seed_context_unref((SeedContext) ctx);
}

// "Leaky" in that it exists for lifetime of program,
// kind of unavoidable though.
static ffi_closure *seed_make_rl_closure(SeedObject function)
{
  ffi_cif *cif;
  ffi_closure *closure;
  ffi_arg result;
  ffi_status status;

  cif = g_new0(ffi_cif, 1);
  closure = mmap(0, sizeof(ffi_closure), PROT_READ | PROT_WRITE |
		 PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
  ffi_prep_cif(cif, FFI_DEFAULT_ABI, 0, &ffi_type_sint, 0);
  ffi_prep_closure(closure, cif, seed_handle_rl_closure, function);

  return closure;
}

static SeedValue
seed_readline_bind(SeedContext ctx,
		   SeedObject function,
		   SeedObject this_object,
		   size_t argument_count,
		   const SeedValue arguments[], SeedValue * exception)
{
  gchar *key;
  ffi_closure *c;
  
  CHECK_ARG_COUNT("readline.bind", 2);

  key = seed_value_to_string(ctx, arguments[0], exception);
  c = seed_make_rl_closure((SeedObject) arguments[1]);

  rl_bind_key(*key, (Function *) c);

  g_free(key);

  return seed_make_null(ctx);
}

static SeedValue
seed_rl_done(SeedContext ctx,
	     SeedObject function,
	     SeedObject this_object,
	     size_t argument_count,
	     const SeedValue arguments[],
	     SeedValue * exception)
{
  CHECK_ARG_COUNT("readline.done", 0);
  rl_done = 1;
  return seed_make_null (ctx);
}

static SeedValue
seed_rl_buffer(SeedContext ctx,
	     SeedObject function,
	     SeedObject this_object,
	     size_t argument_count,
	     const SeedValue arguments[],
	     SeedValue * exception)
{
  CHECK_ARG_COUNT("readline.buffer", 0);
  return seed_value_from_string (ctx, rl_line_buffer, exception);
}

static SeedValue
seed_rl_insert(SeedContext ctx,
	       SeedObject function,
	       SeedObject this_object,
	       size_t argument_count,
	       const SeedValue arguments[],
	       SeedValue * exception)
{
  gchar *ins;
  gint ret;
  
  CHECK_ARG_COUNT("readline.insert", 1);
  
  ins = seed_value_to_string (ctx, arguments[0], exception);
  ret = rl_insert_text (ins);
  g_free (ins);

  return seed_value_from_int (ctx, ret, exception);
}

static SeedValue
seed_readline(SeedContext ctx,
	      SeedObject function,
	      SeedObject this_object,
	      size_t argument_count,
	      const SeedValue arguments[], SeedValue * exception)
{
  SeedValue valstr = 0;
  gchar *str = NULL;
  gchar *buf;
  const gchar *histfname = g_get_home_dir();
  gchar *path = g_build_filename(histfname, ".seed_history", NULL);

  if (!readline_has_initialized)
    {
      read_history(path);
      readline_has_initialized = TRUE;
    }

  CHECK_ARG_COUNT("readline.readline", 1);

  buf = seed_value_to_string(ctx, arguments[0], exception);

  str = readline(buf);
  if (str && *str)
    {
      add_history(str);
      valstr = seed_value_from_string(ctx, str, exception);
      g_free(str);
    }

  write_history(path);
  history_truncate_file(path, 1000);

  g_free(buf);
  g_free(path);

  if (valstr == 0)
    valstr = seed_make_null(ctx);

  return valstr;
}

seed_static_function readline_funcs[] = {
	{"readline", seed_readline, 0},
	{"bind", seed_readline_bind, 0},
	{"done", seed_rl_done, 0},
	{"buffer", seed_rl_buffer, 0},
	{"insert", seed_rl_insert, 0}
};

SeedObject
seed_module_init(SeedEngine * local_eng)
{
  SeedGlobalContext ctx = local_eng->context;
  
  seed_class_definition readline_ns_class_def = seed_empty_class;
  readline_ns_class_def.static_functions = readline_funcs;
  
  SeedClass readline_ns_class = seed_create_class(&readline_ns_class_def);

  eng = local_eng;

  namespace_ref = seed_make_object(eng->context, readline_ns_class, 0);

  return namespace_ref;
}

