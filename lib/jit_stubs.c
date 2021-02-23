#define CAML_INTERNALS

#include "caml/mlvalues.h"
#include "caml/memory.h"
#include "caml/stack.h"
#include "caml/callback.h"
#include "caml/alloc.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

CAMLprim value jit_memalign(value section_size) {
  CAMLparam1 (section_size);
  CAMLlocal1 (result);
  void *addr = NULL;
  int res, size;

  size = Int_val(section_size);
  res = posix_memalign(&addr, 16, section_size);
  if (res) {
    result = caml_alloc(1, 1);
    Store_field(result, 0, Val_int(res));
  } else {
    result = caml_alloc(1, 0);
    Store_field(result, 0, caml_copy_nativeint((intnat) addr));
  };

  CAMLreturn(result);
}

CAMLprim void jit_load_section(value addr, value section, value section_size) {
  CAMLparam3 (addr, section, section_size);
  int size = Int_val(section_size);
  const char *src = String_val(section);
  void *dest = (intnat*) Nativeint_val(addr);

  memcpy(dest, src, size);

  CAMLreturn0;
}

CAMLprim value jit_mprotect_ro(value caml_addr, value caml_size) {
  CAMLparam2 (caml_addr, caml_size);
  CAMLlocal1 (result);

  void *addr;
  int size;

  size = Int_val(caml_size);
  addr = (intnat*) Nativeint_val(caml_addr);

  if (mprotect(addr, size, PROT_READ)) {
    result = caml_alloc(1, 1);
    Store_field(result, 0, Val_int(errno));
  } else {
    result = caml_alloc(1, 0);
    Store_field(result, 0, Val_unit);
  };

  CAMLreturn(result);
}

CAMLprim value jit_mprotect_rx(value caml_addr, value caml_size) {
  CAMLparam2 (caml_addr, caml_size);
  CAMLlocal1 (result);

  void *addr;
  int size;

  size = Int_val(caml_size);
  addr = (intnat*) Nativeint_val(caml_addr);

  if (mprotect(addr, size, PROT_READ | PROT_EXEC)) {
    result = caml_alloc(1, 1);
    Store_field(result, 0, Val_int(errno));
  } else {
    result = caml_alloc(1, 0);
    Store_field(result, 0, Val_unit);
  };

  CAMLreturn(result);
}

static void *addr_from_caml_option(value option)
{
  void *sym = NULL;
  if (Is_block(option)) {
    sym = (intnat*) Nativeint_val(Field(option,0));
  }
  return sym;
}

CAMLprim value jit_run(value symbols_addresses) {
  CAMLparam1 (symbols_addresses);
  CAMLlocal1 (result);
  void *sym,*sym2;
  struct code_fragment * cf;

  void (*entrypoint)(void);

  //sym = optsym("__frametable");
  sym = addr_from_caml_option(Field(symbols_addresses, 0));
  if (NULL != sym) caml_register_frametable(sym);

  //sym = optsym("__gc_roots");
  sym = addr_from_caml_option(Field(symbols_addresses, 1));
  if (NULL != sym) caml_register_dyn_global(sym);

  //sym = optsym("__data_begin");
  //sym2 = optsym("__data_end");
  sym = addr_from_caml_option(Field(symbols_addresses, 2));
  sym2 = addr_from_caml_option(Field(symbols_addresses, 3));
  if (NULL != sym && NULL != sym2)
    caml_page_table_add(In_static_data, sym, sym2);

  //sym = optsym("__code_begin");
  //sym2 = optsym("__code_end");
  sym = addr_from_caml_option(Field(symbols_addresses, 4));
  sym2 = addr_from_caml_option(Field(symbols_addresses, 5));
  if (NULL != sym && NULL != sym2) {
    caml_page_table_add(In_code_area, sym, sym2);
    cf = caml_stat_alloc(sizeof(struct code_fragment));
    cf->code_start = (char *) sym;
    cf->code_end = (char *) sym2;
    cf->digest_computed = 0;
    caml_ext_table_add(&caml_code_fragments_table, cf);
  }

  //entrypoint = optsym("__entry");
  entrypoint = (void*) Nativeint_val(Field(symbols_addresses, 6));
  result = caml_callback((value)(&entrypoint), 0);

  CAMLreturn (result);
}

CAMLprim value jit_run_toplevel(value symbols_addresses) {
  CAMLparam1 (symbols_addresses);
  CAMLlocal2 (res, v);

  res = caml_alloc(1,0);
  v = jit_run(symbols_addresses);
  Store_field(res, 0, v);

  CAMLreturn(res);
}