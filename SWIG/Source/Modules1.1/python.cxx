/* -----------------------------------------------------------------------------
 * python.cxx
 *
 *     Python module.
 *
 * Author(s) : David Beazley (beazley@cs.uchicago.edu)
 *
 * Copyright (C) 1999-2000.  The University of Chicago
 * See the file LICENSE for information on usage and redistribution.
 * ----------------------------------------------------------------------------- */

static char cvsroot[] = "$Header$";

#include "swig11.h"
#include "python.h"

static  String       *const_code = 0;
static  String       *shadow_methods = 0;
static  String       *module = 0;
static  String       *interface = 0;
static  String       *global_name = 0;
static  int           shadow = 0;
static  int           have_defarg = 0;
static  int           have_output;
static  int           use_kw = 0;
static  int           noopt = 1;
static  FILE         *f_shadow;
static  Hash         *hash;
static  Hash         *symbols;
static  String       *classes;
static  String       *func;
static  String       *vars;
static  String       *pragma_include = 0;
static  String       *methods;
static  char         *class_name;

static char *usage = (char *)"\
Python Options (available with -python)\n\
     -globals name   - Set name used to access C global variable ('cvar' by default).\n\
     -module name    - Set module name\n\
     -interface name - Set the lib name\n\
     -keyword        - Use keyword arguments\n\
     -noopt          - No optimized shadows (pre 1.5.2)\n\
     -opt            - Optimized shadow classes (1.5.2 or later)\n\
     -shadow         - Generate shadow classes. \n\n";

/* Test to see if a type corresponds to something wrapped with a shadow class */
static DOH *is_shadow(SwigType *t) {
  DOH *r;
  SwigType *lt = Swig_clocal_type(t);
  r = Getattr(hash,lt);
  Delete(lt);
  return r;
}

/* -----------------------------------------------------------------------------
 * PYTHON::parse_args()
 * ----------------------------------------------------------------------------- */
void
PYTHON::parse_args(int argc, char *argv[]) {
  int i;
  Swig_swiglib_set("python");

  for (i = 1; i < argc; i++) {
      if (argv[i]) {
	  /* Added edz@bsn.com */
	if(strcmp(argv[i],"-interface") == 0) {
            if (argv[i+1]) {
              interface = NewString(argv[i+1]);
              Swig_mark_arg(i);
              Swig_mark_arg(i+1);
              i++;
            } else {
              Swig_arg_error();
            }
	  /* end added */
	  } else if (strcmp(argv[i],"-globals") == 0) {
	    if (argv[i+1]) {
	      global_name = NewString(argv[i+1]);
	      Swig_mark_arg(i);
	      Swig_mark_arg(i+1);
	      i++;
	    } else {
	      Swig_arg_error();
	    }
	  } else if (strcmp(argv[i],"-shadow") == 0) {
	    shadow = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-noopt") == 0) {
	    noopt = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-opt") == 0) {
	    noopt = 0;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-keyword") == 0) {
	    use_kw = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-help") == 0) {
	    fputs(usage,stderr);
	  }
      }
  }
  if (!global_name) global_name = NewString("cvar");
  Preprocessor_define((void *) "SWIGPYTHON", 0);
}

/* -----------------------------------------------------------------------------
 * PYTHON::import()
 * ----------------------------------------------------------------------------- */
void
PYTHON::import(String *modname) {
  if (shadow) {
    Printf(f_shadow,"\nfrom %s import *\n", modname);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::add_method()
 * ----------------------------------------------------------------------------- */
void
PYTHON::add_method(String *name, String *function, int kw) {
  if (!kw)
    Printf(methods,"\t { \"%s\", %s, METH_VARARGS },\n", name, function);
  else
    Printf(methods,"\t { \"%s\", (PyCFunction) %s, METH_VARARGS | METH_KEYWORDS },\n", name, function);
}

/* -----------------------------------------------------------------------------
 * PYTHON::initialize()
 * ----------------------------------------------------------------------------- */
void
PYTHON::initialize(String *modname) {
  char  filen[256];

  hash           = NewHash();
  symbols        = NewHash();
  const_code     = NewString("");
  shadow_methods = NewString("");
  classes        = NewString("");
  func           = NewString("");
  vars           = NewString("");
  pragma_include = NewString("");
  methods        = NewString("");

  Swig_banner(f_runtime);

  Printf(f_runtime,"#define SWIGPYTHON\n");
  if (NoInclude)
    Printf(f_runtime,"#define SWIG_NOINCLUDE\n");

  if (Swig_insert_file("common.swg", f_runtime) == -1) {
    Printf(stderr,"SWIG : Fatal error. Unable to locate common.swg. (Possible installation problem).\n");
    Swig_exit (EXIT_FAILURE);
  }
  if (Swig_insert_file("python.swg", f_runtime) == -1) {
    Printf(stderr,"SWIG : Fatal error. Unable to locate python.swg. (Possible installation problem).\n");
    Swig_exit (EXIT_FAILURE);
  }
  
  if (!module) module = NewString(modname);

  /* If shadow classing is enabled, we're going to change the module name to "modulec" */
  if (shadow) {

    sprintf(filen,"%s%s.py", output_dir, Char(module));
    // If we don't have an interface then change the module name X to Xc
    if (! interface)
    Append(module,"c");
    if ((f_shadow = fopen(filen,"w")) == 0) {
      Printf(stderr,"Unable to open %s\n", filen);
      Swig_exit (EXIT_FAILURE);
    }
    Printf(f_shadow,"# This file was created automatically by SWIG.\n");
    Printf(f_shadow,"import %s\n", interface ? interface : module);

    // Include some information in the code
    Printf(f_header,"\n/*-----------------------------------------------\n              @(target):= %s.so\n\
  ------------------------------------------------*/\n", interface ? interface : module);

    if (!noopt)
      Printf(f_shadow,"import new\n");
  }

  Printf(f_header,"#define SWIG_init    init%s\n\n", module);
  Printf(f_header,"#define SWIG_name    \"%s\"\n", module);

  /* Output the start of the init function.   */
  Printf(f_init,"static PyObject *SWIG_globals;\n");
  Printf(f_init,"#ifdef __cplusplus\n");
  Printf(f_init,"extern \"C\" \n");
  Printf(f_init,"#endif\n");
  Printf(f_init,"SWIGEXPORT(void) init%s(void) {\n",module);
  Printf(f_init,"PyObject *m, *d;\n");
  Printf(f_init,"int i;\n");
  Printf(f_init,"SWIG_globals = SWIG_newvarlink();\n");
  Printf(f_init,"m = Py_InitModule(\"%s\", %sMethods);\n", module, module);
  Printf(f_init,"d = PyModule_GetDict(m);\n");
  Printv(f_init,
	 "for (i = 0; swig_types_initial[i]; i++) {\n",
	 "swig_types[i] = SWIG_TypeRegister(swig_types_initial[i]);\n",
	 "}\n",
	 0);

  Printf(f_wrappers,"#ifdef __cplusplus\n");
  Printf(f_wrappers,"extern \"C\" {\n");
  Printf(f_wrappers,"#endif\n");
  Printf(const_code,"static swig_const_info swig_const_table[] = {\n");
  Printf(methods,"static PyMethodDef %sMethods[] = {\n", module);
}

/* -----------------------------------------------------------------------------
 * PYTHON::close()
 * ----------------------------------------------------------------------------- */
void
PYTHON::close(void) {
  Printf(methods,"\t { NULL, NULL }\n");
  Printf(methods,"};\n");
  Printf(f_wrappers,"%s\n",methods);
  Printf(f_wrappers,"#ifdef __cplusplus\n");
  Printf(f_wrappers,"}\n");
  Printf(f_wrappers,"#endif\n");

  SwigType_emit_type_table(f_runtime,f_wrappers);

  Printf(const_code, "{0}};\n");
  Printf(f_wrappers,"%s\n",const_code);

  Printv(f_init, "SWIG_InstallConstants(d,swig_const_table);\n", 0);
  Printf(f_init,"}\n");

  if (shadow) {
    Printv(f_shadow,
	   classes,
	   "\n\n#-------------- FUNCTION WRAPPERS ------------------\n\n",
	   func,
	   "\n\n#-------------- VARIABLE WRAPPERS ------------------\n\n",
	   vars,
	   0);

    if (Len(pragma_include) > 0) {
      Printv(f_shadow,
	     "\n\n#-------------- USER INCLUDE -----------------------\n\n",
             pragma_include,
	     0);
    }
    fclose(f_shadow);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::get_pointer()
 * ----------------------------------------------------------------------------- */
void
PYTHON::get_pointer(char *src, char *dest, SwigType *t, String *f, char *ret) {
  SwigType *lt;
  SwigType_remember(t);
  Printv(f,tab4, "if ((SWIG_ConvertPtr(", src, ",(void **) &", dest, ",", 0);

  lt = Swig_clocal_type(t);
  if (Cmp(lt,"p.void") == 0) {
    Printv(f, "0,1)) == -1) return ", ret, ";\n", 0);
  }
  /*  if (SwigType_type(t) == T_VOID) Printv(f, "0,1)) == -1) return ", ret, ";\n", 0);*/
  else {
    Printv(f,"SWIGTYPE", SwigType_manglestr(t), ",1)) == -1) return ", ret, ";\n", 0);
  }
  Delete(lt);
}

/* -----------------------------------------------------------------------------
 * PYTHON::create_command()
 * ----------------------------------------------------------------------------- */
void
PYTHON::create_command(String *cname, String *iname) {
  add_method(iname, Swig_name_wrapper(cname), use_kw);
}

/* -----------------------------------------------------------------------------
 * PYTHON::create_function()
 * ----------------------------------------------------------------------------- */
void
PYTHON::function(DOH *node) {
  char    *name;
  char    *iname;
  SwigType *d;
  ParmList *l;
  Parm    *p;
  int     pcount,i,j;
  char    wname[256];
  char    source[64], target[64], argnum[20];
  char    *usage = 0;
  Wrapper *f;
  String *parse_args;
  String *arglist;
  String *get_pointers;
  String *cleanup;
  String *outarg;
  String *check;
  String *kwargs;
  char     *tm;
  int      numopt = 0;

  name = GetChar(node,"name");
  iname = GetChar(node,"scriptname");
  d = Getattr(node,"type");
  l = Getattr(node,"parms");
  f = NewWrapper();
  parse_args = NewString("");
  arglist = NewString("");
  get_pointers = NewString("");
  cleanup = NewString("");
  outarg = NewString("");
  check = NewString("");
  kwargs = NewString("");

  have_output = 0;

  strcpy(wname,Char(Swig_name_wrapper(iname)));

  if (!use_kw) {
    Printv(f,
	   "static PyObject *", wname,
	   "(PyObject *self, PyObject *args) {",
	   0);
  } else {
    Printv(f,
	   "static PyObject *", wname,
	   "(PyObject *self, PyObject *args, PyObject *kwargs) {",
	   0);
  }
  Wrapper_add_local(f,"resultobj", "PyObject *resultobj");

  /* Get the function usage string for later use */

  usage = usage_func(iname,d,l);

  /* Write code to extract function parameters. */
  pcount = emit_args(node, f);
  if (!use_kw) {
    Printf(parse_args,"    if(!PyArg_ParseTuple(args,\"");
  } else {
    Printf(parse_args,"    if(!PyArg_ParseTupleAndKeywords(args,kwargs,\"");
    Printf(arglist,",kwnames");
  }

  i = 0;
  j = 0;
  numopt = check_numopt(l);
  if (numopt) have_defarg = 1;
  p = l;
  Printf(kwargs,"{ ");
  while (p != 0) {
    SwigType *pt = Gettype(p);
    String   *pn = Getname(p);
    String   *pv = Getvalue(p);

    sprintf(source,"obj%d",i);
    sprintf(target,Char(Getlname(p)));
    sprintf(argnum,"%d",j+1);

    if (!Getignore(p)) {
      Putc(',',arglist);
      if (j == pcount-numopt) Putc('|',parse_args);   /* Optional argument separator */
      if (Len(pn)) {
	Printf(kwargs,"\"%s\",", pn);
      } else {
	Printf(kwargs,"\"arg%d\",", j+1);
      }

      if ((tm = Swig_typemap_lookup((char*)"in",pt,pn,source,target,f))) {
	Putc('O',parse_args);
	Wrapper_add_localv(f, source, "PyObject *",source, " = 0", 0);
	Printf(arglist,"&%s",source);
	if (i >= (pcount-numopt))
	  Printv(get_pointers, tab4, "if (", source, ")\n", 0);
	Printv(get_pointers,tm,"\n", 0);
	Replace(get_pointers,"$argnum", argnum, DOH_REPLACE_ANY);
	Replace(get_pointers,"$arg",source, DOH_REPLACE_ANY);
      } else {
	int noarg = 0;

	switch(SwigType_type(pt)) {
	case T_INT : case T_UINT:
	  Putc('i',parse_args);
	  break;
	case T_SHORT: case T_USHORT:
	  Putc('h',parse_args);
	  break;
	case T_LONG : case T_ULONG:
	  Putc('l',parse_args);
	  break;
	case T_SCHAR : case T_UCHAR :
	  Putc('b',parse_args);
	  break;
	case T_CHAR:
	  Putc('c',parse_args);
	  break;
	case T_FLOAT :
	  Putc('f',parse_args);
	  break;
	case T_DOUBLE:
	  Putc('d',parse_args);
	  break;

	case T_BOOL:
	  {
	    char tempb[128];
	    char tempval[128];
	    if (pv) {
	      sprintf(tempval, "(int) %s", Char(pv));
	    }
	    sprintf(tempb,"tempbool%d",i);
	    Putc('i',parse_args);
	    if (!pv)
	      Wrapper_add_localv(f,tempb,"int",tempb,0);
	    else
	      Wrapper_add_localv(f,tempb,"int",tempb, "=",tempval,0);
	    Printv(get_pointers, tab4, target, " = ( ", tempb, " != 0);\n", 0);
	    Printf(arglist,"&%s",tempb);
	    noarg = 1;
	  }
	  break;

	case T_VOID :
	  noarg = 1;
	  break;

	case T_USER:

	  Putc('O',parse_args);
	  sprintf(source,"argo%d", i);
	  sprintf(target,Char(Getlname(p)));

	  Wrapper_add_localv(f,source,"PyObject *",source,"=0",0);
	  Printf(arglist,"&%s",source);
	  SwigType_add_pointer(pt);
	  get_pointer(source, target, pt, get_pointers, (char*)"NULL");
	  SwigType_del_pointer(pt);
	  noarg = 1;
	  break;

	case T_STRING:
	  Putc('s',parse_args);
	  Printf(arglist,"&%s", Getlname(p));
	  noarg = 1;
	  break;

	case T_POINTER: case T_ARRAY: case T_REFERENCE:

	  /* Have some sort of pointer variable.  Create a temporary local
	     variable for the string and read the pointer value into it. */

	  Putc('O',parse_args);
	  sprintf(source,"argo%d", i);
	  sprintf(target,"%s",Char(Getlname(p)));

	  Wrapper_add_localv(f,source,"PyObject *",source,"=0",0);
	  Printf(arglist,"&%s",source);
	  get_pointer(source, target, pt, get_pointers, (char*)"NULL");
	  noarg = 1;
	  break;

	default :
	  Printf(stderr,"%s:%d. Unable to use type %s as a function argument.\n",Getfile(node), Getline(node), SwigType_str(pt,0));
	  break;
	}

	if (!noarg)
	  Printf(arglist,"&%s",Getlname(p));
      }
      j++;
    }
    /* Check if there was any constraint code */
    if ((tm = Swig_typemap_lookup((char*)"check",pt,pn,source,target,0))) {
      Printf(check,"%s\n",tm);
      Replace(check,"$argnum", argnum, DOH_REPLACE_ANY);
    }
    /* Check if there was any cleanup code */
    if ((tm = Swig_typemap_lookup((char*)"freearg",pt,pn,target,source,0))) {
      Printf(cleanup,"%s\n",tm);
      Replace(cleanup,"$argnum", argnum, DOH_REPLACE_ANY);
      Replace(cleanup,"$arg",source, DOH_REPLACE_ANY);
    }
    /* Check for output arguments */
    if ((tm = Swig_typemap_lookup((char*)"argout",pt,pn,target,(char*)"resultobj",0))) {
      Printf(outarg,"%s\n", tm);
      Replace(outarg,"$argnum",argnum,DOH_REPLACE_ANY);
      Replace(outarg,"$arg",source, DOH_REPLACE_ANY);
      have_output++;
    }
    p = Getnext(p);
    i++;
  }

  Printf(kwargs," NULL }");
  if (use_kw) {
    Wrapper_add_localv(f,"kwnames","char *kwnames[] = ", kwargs, ";\n", 0);
    /*    Printv(f->locals,tab4, "char *kwnames[] = ", kwargs, ";\n", 0); */
  }

  Printf(parse_args,":%s\"", iname);
  Printv(parse_args,
	 arglist, ")) return NULL;\n",
	 0);

  /* Now slap the whole first part of the wrapper function together */
  Printv(f, parse_args, get_pointers, check, 0);

  /* Emit the function call */
  emit_func_call(node,f);

  /* Return the function value */
  if ((tm = Swig_typemap_lookup((char*)"out",d,iname,(char*)"result",(char*)"resultobj",0))) {
    Printf(f,"%s\n", tm);
  } else {
    switch(SwigType_type(d)) {
    case T_INT: case T_UINT: case T_BOOL:
    case T_SHORT: case T_USHORT:
    case T_LONG : case T_ULONG:
    case T_SCHAR: case T_UCHAR :
      Printf(f,"    resultobj = PyInt_FromLong((long)result);\n");
      break;
    case T_DOUBLE :
    case T_FLOAT :
      Printf(f,"    resultobj = PyFloat_FromDouble(result);\n");
      break;
    case T_CHAR :
      Printf(f,"    resultobj = Py_BuildValue(\"c\",result);\n");
      break;
    case T_USER :
      SwigType_add_pointer(d);
      SwigType_remember(d);
      Printv(f,tab4, "resultobj = SWIG_NewPointerObj((void *)result, SWIGTYPE", SwigType_manglestr(d), ");\n",0);
      SwigType_del_pointer(d);
      break;
    case T_STRING:
      Printf(f,"    resultobj = Py_BuildValue(\"s\",result);\n");
      break;
    case T_POINTER: case T_ARRAY: case T_REFERENCE:
      SwigType_remember(d);
      Printv(f, tab4, "resultobj = SWIG_NewPointerObj((void *) result, SWIGTYPE", SwigType_manglestr(d), ");\n", 0);
      break;
    case T_VOID:
      Printf(f,"    Py_INCREF(Py_None);\n");
      Printf(f,"    resultobj = Py_None;\n");
      break;
    default :
      Printf(stderr,"%s:%d. Unable to use return type %s in function %s.\n", Getfile(node), Getline(node), SwigType_str(d,0), name);
      break;
    }
  }

  /* Output argument output code */
  Printv(f,outarg,0);

  /* Output cleanup code */
  Printv(f,cleanup,0);

  /* Look to see if there is any newfree cleanup code */
  if (NewObject) {
    if ((tm = Swig_typemap_lookup((char*)"newfree",d,iname,(char*)"result",(char*)"",0))) {
      Printf(f,"%s\n",tm);
    }
  }

  /* See if there is any return cleanup code */
  if ((tm = Swig_typemap_lookup((char*)"ret",d,iname,(char*)"result",(char*)"",0))) {
    Printf(f,"%s\n",tm);
  }

  Printf(f,"    return resultobj;\n}\n");

  /* Substitute the cleanup code */
  Replace(f,"$cleanup",cleanup, DOH_REPLACE_ANY);

  /* Substitute the function name */
  Replace(f,"$name",iname, DOH_REPLACE_ANY);

  /* Dump the function out */
  Printf(f_wrappers,"%s",f);

  /* Now register the function with the interpreter.   */
  add_method(iname, wname, use_kw);

  /* Create a shadow for this function (if enabled and not in a member function) */

  if ((shadow) && (!(shadow & PYSHADOW_MEMBER))) {
    int need_wrapper = 0;
    int munge_return = 0;

    /* Check return code for modification */
    if (is_shadow(d)) {
      need_wrapper = 1;
      munge_return = 1;
    }

    /* If no modification is needed. We're just going to play some
       symbol table games instead */

    if (!need_wrapper) {
      Printv(func,iname, " = ", module, ".", iname, "\n\n", 0);
    } else {
      Printv(func,"def ", iname, "(*args, **kwargs):\n", 0);
      Printv(func, tab4, "val = apply(", module, ".", iname, ",args,kwargs)\n",0);

      if (munge_return) {
	/*  If the output of this object has been remapped in any way, we're
	    going to return it as a bare object */

	if (!Swig_typemap_search((char*)"out",d,iname)) {

	  /* If there are output arguments, we are going to return the value
             unchanged.  Otherwise, emit some shadow class conversion code. */

	  if (!have_output) {
	    Printv(func, tab4, "if val: val = ", is_shadow(d), "Ptr(val)", 0);
	    if ((!SwigType_ispointer(d)) || NewObject)
	      Printf(func, "; val.thisown = 1\n");
	    else
	      Printf(func,"\n");
	  }
	}
      }
      Printv(func, tab4, "return val\n\n", 0);
    }
  }
  Delete(parse_args);
  Delete(arglist);
  Delete(get_pointers);
  Delete(cleanup);
  Delete(outarg);
  Delete(check);
  Delete(kwargs);
  Delete(f);
}

/* -----------------------------------------------------------------------------
 * PYTHON::variable()
 * ----------------------------------------------------------------------------- */
void
PYTHON::variable(DOH *node) {
    char *name, *iname;
    SwigType *t;
    char   *wname;
    static int have_globals = 0;
    char   *tm;
    Wrapper *getf, *setf;

    name = GetChar(node,"name");
    iname = GetChar(node,"scriptname");
    t = Getattr(node,"type");

    getf = NewWrapper();
    setf = NewWrapper();

    /* If this is our first call, add the globals variable to the
       Python dictionary. */

    if (!have_globals) {
      Printf(f_init,"\t PyDict_SetItemString(d,\"%s\", SWIG_globals);\n",global_name);
      have_globals=1;
      if ((shadow) && (!(shadow & PYSHADOW_MEMBER))) {
	Printv(vars, global_name, " = ", module, ".", global_name, "\n", 0);
      }
    }

    wname = Char(Swig_name_wrapper(name));

    /* Create a function for setting the value of the variable */

    Printf(setf,"static int %s_set(PyObject *val) {\n", wname);
    if (!ReadOnly) {
      if ((tm = Swig_typemap_lookup((char*)"varin",t,name,(char*)"val",name,0))) {
	Printf(setf,"%s\n",tm);
	Replace(setf,"$name",iname, DOH_REPLACE_ANY);
      } else {
	switch(SwigType_type(t)) {

	case T_INT: case T_SHORT: case T_LONG :
	case T_UINT: case T_USHORT: case T_ULONG:
	case T_SCHAR: case T_UCHAR: case T_BOOL:
	  Wrapper_add_localv(setf,"tval",SwigType_lstr(t,0),"tval",0);
	  Printv(setf,
		 "tval = (", SwigType_lstr(t,0), ") PyInt_AsLong(val);\n",
		 "if (PyErr_Occurred()) {\n",
		 "PyErr_SetString(PyExc_TypeError,\"C variable '",
		 iname, "'(", SwigType_str(t,0), ")\");\n",
		 "return 1; \n",
		 "}\n",
		 name, " = tval;\n",
		 0);
	  break;

	case T_FLOAT: case T_DOUBLE:
	  Wrapper_add_localv(setf,"tval",SwigType_lstr(t,0), "tval",0);
	  Printv(setf,
		 "tval = (", SwigType_lstr(t,0), ") PyFloat_AsDouble(val);\n",
		 "if (PyErr_Occurred()) {\n",
		 "PyErr_SetString(PyExc_TypeError,\"C variable '",
		 iname, "'(", SwigType_str(t,0), ")\");\n",
		 "return 1; \n",
		 "}\n",
		 name, " = tval;\n",
		 0);
	  break;

	case T_CHAR:
	  Wrapper_add_local(setf,"tval","char * tval");
	  Printv(setf,
		 "tval = (char *) PyString_AsString(val);\n",
		 "if (PyErr_Occurred()) {\n",
		 "PyErr_SetString(PyExc_TypeError,\"C variable '",
		 iname, "'(", SwigType_str(t,0), ")\");\n",
		 "return 1; \n",
		 "}\n",
		 name, " = *tval;\n",
		 0);
	  break;

	case T_USER:
	  SwigType_add_pointer(t);
	  Wrapper_add_localv(setf,"temp",SwigType_lstr(t,0),"temp",0);
	  get_pointer((char*)"val",(char*)"temp",t,setf,(char*)"1");
	  Printv(setf, name, " = *temp;\n", 0);
	  SwigType_del_pointer(t);
	  break;

	case T_STRING:
	  Wrapper_add_local(setf,"tval","char * tval");
	  Printv(setf,
		 "tval = (char *) PyString_AsString(val);\n",
		 "if (PyErr_Occurred()) {\n",
		 "PyErr_SetString(PyExc_TypeError,\"C variable '",
		 iname, "'(", SwigType_str(t,0), ")\");\n",
		 "return 1; \n",
		 "}\n",
		 0);

	  if (CPlusPlus) {
	    Printv(setf,
		   "if (", name, ") delete [] ", name, ";\n",
		   name, " = new char[strlen(tval)+1];\n",
		   "strcpy((char *)", name, ",tval);\n",
		   0);
	  } else {
	    Printv(setf,
		   "if (", name, ") free((char*)", name, ");\n",
		   name, " = (char *) malloc(strlen(tval)+1);\n",
		   "strcpy((char *)", name, ",tval);\n",
		   0);
	  }
	  break;

	case T_ARRAY:
	  {
	    int setable = 0;
	    SwigType *aop;
	    SwigType *ta = Copy(t);
	    aop = SwigType_pop(ta);
	    if (SwigType_type(ta) == T_CHAR) {
	      String *dim = SwigType_array_getdim(aop,0);
	      if (dim && Len(dim)) {
		Printf(setf, "strncpy(%s,PyString_AsString(val), %s);\n", name,dim);
		setable = 1;
	      }
	    }
	    if (!setable) {
	      Printv(setf,
		     "PyErr_SetString(PyExc_TypeError,\"Variable ", iname,
		     " is read-only.\");\n",
		     "return 1;\n",
		     0);
	    }
	    Delete(ta);
	    Delete(aop);
	  }
	  break;

	case T_POINTER: case T_REFERENCE:
	  Wrapper_add_localv(setf,"temp", SwigType_lstr(t,0), "temp",0);
	  get_pointer((char*)"val",(char*)"temp",t,setf,(char*)"1");
	  Printv(setf, name, " = temp;\n", 0);
	  break;

	default:
	  Printf(stderr,"%s:%d. Unable to link with type %s.\n", Getfile(node), Getline(node), SwigType_str(t,0));
	}
      }
      Printf(setf,"    return 0;\n");
    } else {
      /* Is a readonly variable.  Issue an error */
      Printv(setf,
	     "PyErr_SetString(PyExc_TypeError,\"Variable ", iname,
	     " is read-only.\");\n",
	     "return 1;\n",
	     0);
    }

    Printf(setf,"}\n");
    Printf(f_wrappers,"%s", setf);

    /* Create a function for getting the value of a variable */
    Printf(getf,"static PyObject *%s_get() {\n", wname);
    Wrapper_add_local(getf,"pyobj", "PyObject *pyobj");
    if ((tm = Swig_typemap_lookup((char*)"varout",t,name,name,(char*)"pyobj",0))) {
      Printf(getf,"%s\n",tm);
      Replace(getf,"$name",iname, DOH_REPLACE_ANY);
    } else if ((tm = Swig_typemap_lookup((char*)"out",t,name,name,(char*)"pyobj",0))) {
      Printf(getf,"%s\n",tm);
      Replace(getf,"$name",iname, DOH_REPLACE_ANY);
    } else {
      switch(SwigType_type(t)) {
      case T_INT: case T_UINT:
      case T_SHORT: case T_USHORT:
      case T_LONG: case T_ULONG:
      case T_SCHAR: case T_UCHAR: case T_BOOL:
	Printv(getf, "pyobj = PyInt_FromLong((long) ", name, ");\n", 0);
	break;
      case T_FLOAT: case T_DOUBLE:
	Printv(getf, "pyobj = PyFloat_FromDouble((double) ", name, ");\n", 0);
	break;
      case T_CHAR:
	Wrapper_add_local(getf,"ptemp","char ptemp[2]");
	Printv(getf,
	       "ptemp[0] = ", name, ";\n",
	       "ptemp[1] = 0;\n",
	       "pyobj = PyString_FromString(ptemp);\n",
	       0);
	break;
      case T_USER:
	SwigType_add_pointer(t);
	SwigType_remember(t);
	Printv(getf,
	       "pyobj = SWIG_NewPointerObj((void *) &", name ,
	       ", SWIGTYPE", SwigType_manglestr(t), ");\n",
	       0);
	SwigType_del_pointer(t);
	break;
      case T_STRING:
	Printv(getf,
	       "if (", name, ")\n",
	       "pyobj = PyString_FromString(", name, ");\n",
	       "else pyobj = PyString_FromString(\"(NULL)\");\n",
	       0);
	break;

      case T_POINTER: case T_ARRAY: case T_REFERENCE:
	{
	  SwigType *ta = Copy(t);
	  SwigType_pop(ta);
	  if (SwigType_type(ta) == T_CHAR) {
	    Printv(getf,"pyobj = PyString_FromString(", name, ");\n", 0);
	  } else {
	    SwigType_remember(t);
	    Printv(getf,
		   "pyobj = SWIG_NewPointerObj((void *)", name,
		   ", SWIGTYPE", SwigType_manglestr(t), ");\n",
		   0);
	  }
	  Delete(ta);
	}
	break;

      default:
	Printf(stderr,"Unable to link with type %s\n", SwigType_str(t,0));
	break;
      }
    }

    Printf(getf,"    return pyobj;\n}\n");
    Printf(f_wrappers,"%s", getf);

    /* Now add this to the variable linking mechanism */

    Printf(f_init,"\t SWIG_addvarlink(SWIG_globals,\"%s\",%s_get, %s_set);\n", iname, wname, wname);

    /* Output a shadow variable.  (If applicable and possible) */
    if ((shadow) && (!(shadow & PYSHADOW_MEMBER))) {
      if (is_shadow(t)) {
	Printv(vars,
	       iname, " = ", is_shadow(t), "Ptr(", module, ".", global_name,
	       ".", iname, ")\n",
	       0);
      }
    }
    Delete(setf);
    Delete(getf);
}

/* -----------------------------------------------------------------------------
 * PYTHON::constant()
 * ----------------------------------------------------------------------------- */
void
PYTHON::constant(DOH *node) {
  char   *name;
  SwigType *type;
  char   *value;
  char   *tm;

  name = GetChar(node,"name");
  type = Getattr(node,"type");
  value = GetChar(node,"value");
  
  if ((tm = Swig_typemap_lookup((char*)"const",type,name,value,name,0))) {
    Printf(const_code,"%s\n", tm);
  } else {
    switch(SwigType_type(type)) {
    case T_INT: case T_UINT: case T_BOOL:
    case T_SHORT: case T_USHORT:
    case T_LONG: case T_ULONG:
    case T_SCHAR: case T_UCHAR:
      Printv(const_code, "{ SWIG_PY_INT,     \"", name, "\", (long) ", value, ", 0, 0, 0},\n", 0);
      break;
    case T_DOUBLE:
    case T_FLOAT:
      Printv(const_code, "{ SWIG_PY_FLOAT,   \"", name, "\", 0, (double) ", value, ", 0,0},\n", 0);
      break;
    case T_CHAR :
      Printf(const_code, "{ SWIG_PY_STRING, \"%s\", 0, 0, (void *) \"%s\", 0 }, \n", name, value);
      break;
    case T_STRING:
      Printf(const_code,"{ SWIG_PY_STRING, \"%s\", 0, 0, (void *) \"%s\", 0 }, \n", name, value);
      break;
    case T_POINTER: case T_ARRAY: case T_REFERENCE:
      SwigType_remember(type);
      Printv(const_code, "{ SWIG_PY_POINTER, \"", name, "\", 0, 0, (void *) ", value, ", &SWIGTYPE", SwigType_manglestr(type), "}, \n", 0);
      break;
    default:
      Printf(stderr,"%s:%d. Unsupported constant value. %s, %d\n", Getfile(node), Getline(node), name, SwigType_type(type));
      return;
      break;
    }
  }
  if ((shadow) && (!(shadow & PYSHADOW_MEMBER))) {
    Printv(vars,name, " = ", module, ".", name, "\n", 0);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::usage_func() - Make string showing how to call a function
 * ----------------------------------------------------------------------------- */
char *
PYTHON::usage_func(char *iname, SwigType *, ParmList *l) {
  static String *temp = 0;
  Parm  *p;
  int    i;
  if (!temp) temp = NewString("");

  Clear(temp);
  Printf(temp,"%s(", iname);

  i = 0;
  p = l;
  while (p != 0) {
    SwigType *pt = Gettype(p);
    String   *pn = Getname(p);
    if (!Getignore(p)) {
      i++;
      /* If parameter has been named, use that.   Otherwise, just print a type  */

      if (SwigType_type(pt) != T_VOID) {
	if (Len(pn) > 0) {
	  Printf(temp,"%s",pn);
	} else {
	  Printf(temp,"%s", SwigType_str(pt,0));
	}
      }
      p = Getnext(p);
      if (p != 0) {
	if (!Getignore(p))
	  Putc(',',temp);
      }
    } else {
      p = Getnext(p);
      if (p) {
	if ((!Getignore(p)) && (i > 0))
	  Putc(',',temp);
      }
    }
  }
  Putc(')',temp);
  return Char(temp);
}

/* -----------------------------------------------------------------------------
 * PYTHON::nativefunctin()
 * ----------------------------------------------------------------------------- */
void
PYTHON::nativefunction(DOH *node) {
  char *name;
  char *funcname;
  name = GetChar(node,"scriptname");
  funcname = GetChar(node,"name");

  /* Figure out what kind of function this is */
  if (Swig_proto_cmp("f(p.PyObject,p.PyObject).p.PyObject",node) == 0) {
    /* Not with keyword arguments */
    add_method(name,funcname,0);
  } 
  if (Swig_proto_cmp("f(p.PyObject,p.PyObject,p.PyObject).p.PyObject",node) == 0) {
    add_method(name,funcname,1);
  }
  if (shadow) {
    Printv(func, name, " = ", module, ".", name, "\n\n", 0);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_class_decl() - Register a class definition
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_class_decl(DOH *node) {
  String *name = Getname(node);
  String *rename = Getattr(node,"scriptname");
  String *ctype  = Getattr(node,"classtype");
  String *stype;
  if (shadow) {
    stype = NewString(name);
    SwigType_add_pointer(stype);
    Setattr(hash,stype,rename);
    Delete(stype);
    /* Add full name of datatype to the hash table */
    if (Len(ctype) > 0) {
	stype = NewStringf("%s %s", ctype, name);
	SwigType_add_pointer(stype);
	Setattr(hash,stype,rename);
	Delete(stype);
    }
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::pragma()
 * ----------------------------------------------------------------------------- */
void
PYTHON::pragma(DOH *node) {
  String *name;
  String *value;
  name = Getattr(node,"name");
  value = Getattr(node,"value");

  if (Cmp(name,"code") == 0) {
    if (shadow) {
      Printf(f_shadow,"%s\n",value);
    }
  } else if (Cmp(name,"include") == 0) {
    if (shadow) {
      if (value) {
	FILE *f = Swig_open(value);
	if (!f) {
	  Printf(stderr,"%s:%d. Unable to locate file %s\n", Getfile(node), Getline(node),value);
	} else {
	  char buffer[4096];
	  while (fgets(buffer,4095,f)) {
	    Printv(pragma_include,buffer,0);
	  }
	}
      }
    }
  }
}

#ifdef OLD
/* C++ pragmas are no longer handled as a special case.  The generic %pragma
   directive may used in or outside of a class.  Therefore, this functionality
   should be moved into the pragma method, not handled here.  -- Dave. (12/14/00).
*/
   
/* -----------------------------------------------------------------------------
 * PYTHON::cpp_pragma() - Handle C++ pragmas
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_pragma(Pragma *plist) {
  PyPragma *pyp1 = 0, *pyp2 = 0;
  if (pragmas) {
    delete pragmas;
    pragmas = 0;
  }
  while (plist) {
    if (strcmp(Char(plist->lang),(char*)"python") == 0) {
      if (strcmp(Char(plist->name),"addtomethod") == 0) {
	/* parse value, expected to be in the form "methodName:line" */
	String *temp = NewString(plist->value);
	char* txtptr = strchr(Char(temp), ':');
	if (txtptr) {
	  /* add name and line to a list in current_class */
	  *txtptr = 0;
	  txtptr++;
	  pyp1 = new PyPragma(Char(temp),txtptr);
	  pyp1->next = 0;
	  if (pyp2) {
	      pyp2->next = pyp1;
	      pyp2 = pyp1;
	  } else {
	    pragmas = pyp1;
	    pyp2 = pragmas;
	  }
	} else {
	  Printf(stderr,"%s : Line %d. Malformed addtomethod pragma.  Should be \"methodName:text\"\n",
		  Char(plist->filename),plist->lineno);
	}
	Delete(temp);
      } else if (strcmp(Char(plist->name), "addtoclass") == 0) {
	pyp1 = new PyPragma((char*)"__class__",Char(plist->value));
	pyp1->next = 0;
	if (pyp2) {
	  pyp2->next = pyp1;
	  pyp2 = pyp1;
	} else {
	  pragmas = pyp1;
	  pyp2 = pragmas;
	}
      }
    }
    plist = plist->next;
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::emitAddPragmas()
 *
 * Search the current class pragma for any text belonging to name.
 * Append the text properly spaced to the output string.
 * ----------------------------------------------------------------------------- */
void
PYTHON::emitAddPragmas(String *output, char* name, char* spacing) {
  PyPragma *p = pragmas;
  while (p) {
    if (strcmp(Char(p->m_method),name) == 0) {
      Printv(output,spacing,p->m_text,"\n",0);
    }
    p = p->next;
  }
}

#endif

/* C++ Support + Shadow Classes */

static  String   *setattr = 0;
static  String   *getattr = 0;
static  String   *csetattr = 0;
static  String   *cgetattr = 0;
static  String   *pyclass = 0;
static  String   *imethod = 0;
static  String   *construct = 0;
static  String   *cinit = 0;
static  String   *additional = 0;
static  int       have_constructor;
static  int       have_destructor;
static  int       have_getattr;
static  int       have_setattr;
static  int       have_repr;
static  char     *class_type;
static  char     *real_classname;
static  String   *base_class = 0;
static  int       class_renamed = 0;

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_open_class()
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_open_class(DOH *node) {
  this->Language::cpp_open_class(node);

  char *classname = GetChar(node,"name");
  char *rname = GetChar(node,"scriptname");
  char *ctype = GetChar(node,"classtype");
  
  if (shadow) {
    /* Create new strings for building up a wrapper function */
    setattr   = NewString("");
    getattr   = NewString("");
    csetattr  = NewString("");
    cgetattr  = NewString("");
    pyclass   = NewString("");
    imethod   = NewString("");
    construct = NewString("");
    cinit     = NewString("");
    additional= NewString("");
    base_class = 0;
    have_constructor = 0;
    have_destructor = 0;
    have_getattr = 0;
    have_setattr = 0;
    have_repr = 0;
    if (rname) {
      class_name = Swig_copy_string(rname);
      class_renamed = 1;
    } else {
      class_name = Swig_copy_string(classname);
      class_renamed = 0;
    }
  }

  real_classname = Swig_copy_string(classname);
  class_type = Swig_copy_string(ctype);

  cpp_class_decl(node);

  if (shadow) {
    Printv(setattr,
	   tab4, "def __setattr__(self,name,value):\n",
	   tab8, "if (name == \"this\") or (name == \"thisown\"): self.__dict__[name] = value; return\n",
	   tab8, "method = ", class_name, ".__setmethods__.get(name,None)\n",
	   tab8, "if method: return method(self,value)\n",
	   0);

    Printv(getattr, tab4, "def __getattr__(self,name):\n", 0);
    Printv(csetattr, tab4, "__setmethods__ = {\n", 0);
    Printv(cgetattr, tab4, "__getmethods__ = {\n", 0);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_member_func()
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_memberfunction(DOH *node) {
  char *realname;
  char *name, *iname;
  SwigType *t;
  ParmList *l;
  int   oldshadow;
  char  cname[1024];

  /* Create the default member function */
  oldshadow = shadow;    /* Disable shadowing when wrapping member functions */
  if (shadow) shadow = shadow | PYSHADOW_MEMBER;
  this->Language::cpp_memberfunction(node);
  shadow = oldshadow;
  if (shadow) {
    name = GetChar(node,"name");
    iname = GetChar(node,"scriptname");
    t = Getattr(node,"type");
    l = Getattr(node,"parms");
    realname = iname ? iname : name;

    /* Check to see if we've already seen this */
    sprintf(cname,"python:%s::%s",class_name,realname);
    if (Getattr(symbols,cname)) return;
    Setattr(symbols,cname,cname);

    if (strcmp(realname,"__repr__") == 0)
      have_repr = 1;

    if (!is_shadow(t) && !noopt) {
      Printv(imethod,
	     class_name, ".", realname, " = new.instancemethod(", module, ".", Swig_name_member(class_name,realname), ", None, ", class_name, ")\n",
	     0);
    } else {
      if (use_kw)
	Printv(pyclass,tab4, "def ", realname, "(*args, **kwargs):\n", 0);
      else
	Printv(pyclass, tab4, "def ", realname, "(*args):\n", 0);

      if (use_kw)
	Printv(pyclass, tab8, "val = apply(", module, ".", Swig_name_member(class_name,realname), ",args, kwargs)\n", 0);
      else
	Printv(pyclass, tab8, "val = apply(", module, ".", Swig_name_member(class_name,realname), ",args)\n",0);

      /* Check to see if the return type is an object */
      if (is_shadow(t)) {
	if (!Swig_typemap_search((char*)"out",t,Swig_name_member(class_name,realname))) {
	  if (!have_output) {
	    Printv(pyclass, tab8, "if val: val = ", is_shadow(t), "Ptr(val) ", 0);
	    if ((!SwigType_ispointer(t) || NewObject)) {
	      Printf(pyclass, "; val.thisown = 1\n");
	    } else {
	      Printf(pyclass,"\n");
	    }
	  }
	}
      }
      Printv(pyclass, tab8, "return val\n", 0);
    }
    /*    emitAddPragmas(*pyclass, realname, tab8);
	  *pyclass << tab8 << "return val\n"; */
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_constructor()
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_constructor(DOH *node) {
  char *name, *iname;
  ParmList *l;
  char *realname;
  int   oldshadow = shadow;
  char  cname[1024];

  if (shadow) shadow = shadow | PYSHADOW_MEMBER;
  this->Language::cpp_constructor(node);
  shadow = oldshadow;

  if (shadow) {
    name = GetChar(node,"name");
    iname = GetChar(node,"scriptname");
    l = Getattr(node,"parms");
    realname = iname ? iname : class_name;

    /* Check to see if we've already seen this */
    sprintf(cname,":python:constructor:%s::%s",class_name,realname);
    if (Getattr(symbols,cname)) return;
    Setattr(symbols,cname,cname);

    if (!have_constructor) {
      if (use_kw)
	Printv(construct, tab4, "def __init__(self,*args,**kwargs):\n", 0);
      else
	Printv(construct, tab4, "def __init__(self,*args):\n",0);

      if (use_kw)
	Printv(construct, tab8, "self.this = apply(", module, ".", Swig_name_construct(realname), ",args,kwargs)\n", 0);
      else
	Printv(construct, tab8, "self.this = apply(", module, ".", Swig_name_construct(realname), ",args)\n", 0);
      Printv(construct, tab8, "self.thisown = 1\n", 0);
      //      emitAddPragmas(construct,(char*)"__init__",(char*)tab8);
      have_constructor = 1;
    } else {
      /* Hmmm. We seem to be creating a different constructor.  We're just going to create a
	 function for it. */

      if (use_kw)
	Printv(additional, "def ", realname, "(*args,**kwargs):\n", 0);
      else
	Printv(additional, "def ", realname, "(*args):\n", 0);

      Printv(additional, tab4, "val = ", class_name, "Ptr(apply(", 0);
      if (use_kw)
	Printv(additional, module, ".", Swig_name_construct(realname), ",args,kwargs))\n", 0);
      else
	Printv(additional, module, ".", Swig_name_construct(realname), ",args))\n", 0);
      Printv(additional,tab4, "val.thisown = 1\n",
	     tab4, "return val\n\n", 0);
    }
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_destructor()
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_destructor(DOH *node) {
  char *name, *newname;
  char *realname;
  int oldshadow = shadow;

  if (shadow) shadow = shadow | PYSHADOW_MEMBER;
  this->Language::cpp_destructor(node);
  shadow = oldshadow;
  if (shadow) {
    name = GetChar(node,"name");
    newname = GetChar(node,"scriptname");
    if (newname) realname = newname;
    else realname = class_renamed ? class_name : name;

    Printv(pyclass, tab4, "def __del__(self,", module, "=", module, "):\n", 0);
    //   emitAddPragmas(pyclass,(char*)"__del__",(char*)tab8);
    Printv(pyclass, tab8, "if self.thisown == 1 :\n",
	   tab8, tab4, module, ".", Swig_name_destroy(realname), "(self)\n", 0);

    have_destructor = 1;
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_close_class() - Close a class and write wrappers
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_close_class() {
  String    *ptrclass;
  String    *repr;

  ptrclass = NewString("");
  repr =  NewString("");

  if (shadow) {
    if (!have_constructor) {
      /* Build a constructor that takes a pointer to this kind of object */
      Printv(construct,
	     tab4, "def __init__(self,this):\n",
	     tab8, "self.this = this\n",
	     0);
    }
    /* First, build the pointer base class */
    if (base_class) {
      Printv(ptrclass, "class ", class_name, "(", base_class, "):\n", 0);
    } else {
      Printv(ptrclass, "class ", class_name, ":\n", 0);
    }
    Printv(getattr,
	   tab8, "method = ", class_name, ".__getmethods__.get(name,None)\n",
	   tab8, "if method: return method(self)\n",
	   tab8, "raise AttributeError,name\n",
	   0);
    Printv(setattr, tab8, "self.__dict__[name] = value\n",0);
    Printv(cgetattr, tab4, "}\n", 0);
    Printv(csetattr, tab4, "}\n", 0);
    Printv(ptrclass,cinit,construct,"\n",0);
    Printv(classes,ptrclass,pyclass,0);

    if (have_setattr) {
      Printv(classes, csetattr, setattr, 0);
    }
    if (have_getattr) {
      Printv(classes,cgetattr,getattr,0);
    }
    if (!have_repr) {
      /* Supply a repr method for this class  */
      Printv(repr,
	     tab4, "def __repr__(self):\n",
	     tab8, "return \"<C ", class_name," instance at %s>\" % (self.this,)\n",
	     0);

      Printv(classes,repr,0);
      //     emitAddPragmas(classes,(char*)"__class__",(char*)tab4);
    }

    /* Now build the real class with a normal constructor */
    Printv(classes,
	   "class ", class_name, "Ptr(", class_name, "):\n",
	   tab4, "def __init__(self,this):\n",
	   tab8, "self.this = this\n",
	   tab8, "self.thisown = 0\n",
	   tab8, "self.__class__ = ", class_name, "\n",
	   "\n", additional, "\n",
	   0);

    Printv(classes,imethod,"\n",0);
    Delete(pyclass);
    Delete(imethod);
    Delete(setattr);
    Delete(getattr);
    Delete(additional);
  }
  Delete(ptrclass);
  Delete(repr);
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_inherit() - Handle inheritance
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_inherit(List *bases) {
  char *bc;
  String *base;
  int   first_base = 0;

  if (!shadow) {
    this->Language::cpp_inherit(bases);
    return;
  }

  /* We'll inherit variables and constants, but not methods */
  this->Language::cpp_inherit(bases);

  if (!bases) return;
  base_class = NewString("");

  /* Now tell the Python module that we're inheriting from a base class */

  for (base = Firstitem(bases); base; base = Nextitem(bases)) {
    String *bs = NewString(base);
    SwigType_add_pointer(bs);
    bc = GetChar(hash,bs);
    if (bc) {
      if (first_base) Putc(',',base_class);
      Printv(base_class,bc,0);
      first_base = 1;
    }
    Delete(bs);
  }
  if (!first_base) {
    Delete(base_class);
    base_class = 0;
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_variable() - Add a member variable
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_variable(DOH *node) {
  char *name, *iname;
  SwigType *t;
  char *realname;
  int   inhash = 0;
  int   oldshadow = shadow;
  char  cname[512];

  if (shadow) shadow = shadow | PYSHADOW_MEMBER;
  this->Language::cpp_variable(node);
  shadow = oldshadow;

  if (shadow) {
    name = GetChar(node,"name");
    iname = GetChar(node,"scriptname");
    t = Getattr(node,"type");
    have_getattr = 1;
    have_setattr = 1;
    realname = iname ? iname : name;

    /* Check to see if we've already seen this */
    sprintf(cname,"python:%s::%s:",class_name,realname);
    if (Getattr(symbols,cname)) return;

    Setattr(symbols,cname,cname);

    /* Figure out if we've seen this datatype before */
    if (is_shadow(t)) inhash = 1;

    if (ReadOnly) {
      /*      *setattr << tab8 << tab4 << "raise RuntimeError, \'Member is read-only\'\n"; */
    } else {
      Printv(csetattr, tab8, "\"", realname, "\" : ", module, ".", Swig_name_set(Swig_name_member(class_name,realname)), ",\n", 0);
    }
    if (inhash) {
      Printv(cgetattr, tab8, "\"", realname, "\" : lambda x : ", is_shadow(t), "Ptr(", module, ".", Swig_name_get(Swig_name_member(class_name,realname)), "(x)),\n", 0);
    } else {
      Printv(cgetattr, tab8, "\"", realname, "\" : ", module, ".", Swig_name_get(Swig_name_member(class_name,realname)),",\n", 0);
    }
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::cpp_declare_const()
 * ----------------------------------------------------------------------------- */
void
PYTHON::cpp_constant(DOH *node) {
  char *name, *iname, *value;
  SwigType *type;
  char *realname;
  int   oldshadow = shadow;
  char  cname[512];

  if (shadow) shadow = shadow | PYSHADOW_MEMBER;
  this->Language::cpp_constant(node);
  shadow = oldshadow;

  if (shadow) {
    name = GetChar(node,"name");
    iname = GetChar(node,"scriptname");
    type = Getattr(node,"type");
    value = GetChar(node,"value");
    realname = iname ? iname : name;

    /* Check to see if we've already seen this */
    sprintf(cname,"python:%s::%s", class_name, realname);
    if (Getattr(symbols,cname)) return;
    Setattr(symbols,cname,cname);
    Printv(cinit, tab4, realname, " = ", module, ".", Swig_name_member(class_name,realname), "\n", 0);
  }
}

/* -----------------------------------------------------------------------------
 * PYTHON::add_typedef() - Manage typedef's for shadow classes
 * ----------------------------------------------------------------------------- */
void
PYTHON::add_typedef(SwigType *t, String *name) {
  if (!shadow) return;
  if (is_shadow(t)) {
    DOH *node = NewHash();
    Setname(node,name);
    Setattr(node,"scriptname", is_shadow(t));
    Setattr(node,"classtype","");
    cpp_class_decl(node);
  }
}
