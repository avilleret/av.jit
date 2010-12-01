/* 
	Copyright 2010 - Antoine Villeret
	ARToolkit multiple example ported to Jitter
 */

#include "jit.common.h"
#include "max.jit.mop.h"

typedef struct _max_art_jit_simple 
{
	t_object		ob;
	void			*obex;
} t_max_art_jit_simple;

t_jit_err art_jit_simple_init(void); 

void *max_art_jit_simple_new(t_symbol *s, long argc, t_atom *argv);
void max_art_jit_simple_free(t_max_art_jit_simple *x);
void *max_art_jit_simple_class;
		 	
int main(void)
{	
	void *p,*q;
	
	art_jit_simple_init();	
	setup((t_messlist **)&max_art_jit_simple_class, 
		  (method)max_art_jit_simple_new, 
		  (method)max_art_jit_simple_free, 
		  (short)sizeof(t_max_art_jit_simple), 
		  0L, 
		  A_GIMME, 
		  0);

	p = max_jit_classex_setup(calcoffset(t_max_art_jit_simple,obex));
	q = jit_class_findbyname(gensym("art_jit_simple"));    
    max_jit_classex_mop_wrap(p,q,0); 		//name/type/dim/planecount/bang/outputmatrix/etc
    max_jit_classex_standard_wrap(p,q,0); 	//getattributes/dumpout/maxjitclassaddmethods/etc
	
    addmess((method)max_jit_mop_assist, "assist", A_CANT,0);  //standard mop assist fn
	
	return 0;
}

void max_art_jit_simple_free(t_max_art_jit_simple *x)
{
	max_jit_mop_free(x);
	jit_object_free(max_jit_obex_jitob_get(x));
	max_jit_obex_free(x);
}

void *max_art_jit_simple_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_art_jit_simple *x;
	void *o;

	if (x=(t_max_art_jit_simple *)max_jit_obex_new(max_art_jit_simple_class,gensym("art_jit_simple"))) {
		if (o=jit_object_new(gensym("art_jit_simple"))) {
			max_jit_mop_setup_simple(x,o,argc,argv);			
			max_jit_attr_args(x,argc,argv);
		} else {
			jit_object_error((t_object *)x,"art.jit.simple : could not allocate object");
			freeobject((t_object *)x);
			x = NULL;
		}
	}
	return (x);
}
