/*
 Antoine Villeret
 Copyright - 2010
*/

/*
 This file is part of av.jit
 
 av.jit is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 av.jit is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with av.jit.  If not, see <http://www.gnu.org/licenses/>.
 
*/

#include "jit.common.h"
#include "max.jit.mop.h"

typedef struct _max_jit_iidc 
{
	t_object		ob;
	void			*obex;
	t_symbol		*servername;
} t_max_jit_iidc;

t_jit_err jit_iidc_init(void); 

//prototypes
void *max_jit_iidc_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_iidc_notify(t_max_jit_iidc *x, t_symbol *s, t_symbol *msg, void *ob, void *data);
void max_jit_iidc_free(t_max_jit_iidc *x);
void max_jit_iidc_outputmatrix(t_max_jit_iidc *x);

void max_jit_iidc_resetfwbus(t_max_jit_iidc *x);
void max_jit_iidc_getcameraguidlist(t_max_jit_iidc *x);
void max_jit_iidc_getname(t_max_jit_iidc *x);
void max_jit_iidc_getfeature(t_max_jit_iidc *x, t_symbol *s);
void max_jit_iidc_getboundaries(t_max_jit_iidc *x, t_symbol *s);
void max_jit_iidc_getfeaturemodelist(t_max_jit_iidc *x, t_symbol *s);
void max_jit_iidc_getfeaturemode(t_max_jit_iidc *x, t_symbol *s);
void max_jit_iidc_setfeature(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);
void max_jit_iidc_setfeaturemode(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);
void max_jit_iidc_setroi(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);
void *max_jit_iidc_class;
		 	
int main(void)
{	
	void *p,*q;
	
	jit_iidc_init();	
	setup((t_messlist **) &max_jit_iidc_class, 
		  (method)max_jit_iidc_new, 
		  (method)max_jit_iidc_free, 
		  (short)sizeof(t_max_jit_iidc), 
		  0L, A_GIMME, 0);

	p = max_jit_classex_setup(calcoffset(t_max_jit_iidc,obex));
	q = jit_class_findbyname(gensym("jit_iidc")); 
	
	//override the default NOTIFY, OUTPUTMATRIX & JIT_MATRIX methods
	/*
	max_jit_classex_mop_wrap(p,q,MAX_JIT_MOP_FLAGS_OWN_NOTIFY|
							     MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX|
							     MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX); 		//name/type/dim/planecount/bang/outputmatrix/etc
	 */
	max_jit_classex_mop_wrap(p,q, MAX_JIT_MOP_FLAGS_OWN_NOTIFY | MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX|MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX); 		//name/type/dim/planecount/bang/outputmatrix/etc
	//max_jit_classex_mop_wrap(p,q,0);
	max_jit_classex_standard_wrap(p,q,0); 	//getattributes/dumpout/maxjitclassaddmethods/etc
	
	max_addmethod_usurp_low((method)max_jit_iidc_outputmatrix, "outputmatrix");
	//max_addmethod_defer_low((method)max_jit_iidc_getfeaturelist,"getfeaturelist");
	

    addmess((method)max_jit_mop_assist,				"assist",				A_CANT,		0);
	
	addmess((method)max_jit_iidc_notify,			"notify",				A_CANT,		0);		// notify method

	//addmess((method)max_jit_iidc_resetfwbus,		"reset",				0);
	//addmess((method)max_jit_iidc_getcameraguidlist,	"getcameraguidlist",	0);

	addmess((method)max_jit_iidc_getname,			"getmodel",				0);
	addmess((method)max_jit_iidc_getfeature,		"getfeature",			A_DEFSYM,	0);
	addmess((method)max_jit_iidc_getboundaries,		"getboundaries",		A_DEFSYM,	0);
	addmess((method)max_jit_iidc_getfeaturemodelist,"getboundaries",		A_DEFSYM,	0);
	addmess((method)max_jit_iidc_getfeaturemode,	"getboundaries",		A_DEFSYM,	0);
	addmess((method)max_jit_iidc_setfeature,		"setfeature",			A_GIMME,	0);
	addmess((method)max_jit_iidc_setfeaturemode,	"setfeaturemode",		A_GIMME,	0);
	addmess((method)max_jit_iidc_setroi,			"setroi",				A_GIMME,	0);
	return 0;
}

//s is the servername, msg is the message, ob is the server object pointer, 
//and data is extra data the server might provide for a given message
void max_jit_iidc_notify(t_max_jit_iidc *x, t_symbol *s, t_symbol *msg, void *ob, void *data)
{
	if		(msg==gensym("feature"))			max_jit_obex_dumpout(x,msg,	2,						(t_atom *)data);
	else if (msg==gensym("featuremode"))		max_jit_obex_dumpout(x,msg,	1,						(t_atom *)data);
	else if (msg==gensym("boundaries"))			max_jit_obex_dumpout(x,msg,	3,						(t_atom *)data);
	else if (msg==gensym("model"))				max_jit_obex_dumpout(x,msg,	2,						(t_atom *)data);
	else if (msg==gensym("featuremodelist"))	max_jit_obex_dumpout(x,msg,	jit_atom_getlong(data),	(t_atom *)data+1);
	else if (msg==gensym("frameratelist"))		max_jit_obex_dumpout(x,msg,	jit_atom_getlong(data),	(t_atom *)data+1);
	else if (msg==gensym("featuremode"))		max_jit_obex_dumpout(x,msg,	jit_atom_getlong(data),	(t_atom *)data+1);
	else if (msg==gensym("cameraguidlist"))		max_jit_obex_dumpout(x,msg,	jit_atom_getlong(data),	(t_atom *)data+1);
	else if (msg==gensym("videomodelist"))		max_jit_obex_dumpout(x,msg,	jit_atom_getlong(data),	(t_atom *)data+1);
	else max_jit_mop_notify(x,s,msg);
	
}


void max_jit_iidc_outputmatrix(t_max_jit_iidc *x)
{
	long outputmode=max_jit_mop_getoutputmode(x);
	void *mop=max_jit_obex_adornment_get(x,_jit_sym_jit_mop);
	t_jit_err err;
	
	if (outputmode&&mop) { //always output unless output mode is none
		if (outputmode==1) {
			if (err=(t_jit_err)jit_object_method(
				max_jit_obex_jitob_get(x), 
				_jit_sym_matrix_calc,
				jit_object_method(mop,_jit_sym_getinputlist),
				jit_object_method(mop,_jit_sym_getoutputlist)))						
			{
				jit_error_code(x,err); 
			} else {
				max_jit_mop_outputmatrix(x);
			}
		} else {
			max_jit_mop_outputmatrix(x);
		}
	}	
}

void max_jit_iidc_getname(t_max_jit_iidc *x)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("getmodel"));
}

void max_jit_iidc_getfeature(t_max_jit_iidc *x, t_symbol *s)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("getfeature"), s);
}

void max_jit_iidc_getboundaries(t_max_jit_iidc *x, t_symbol *s)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("getboundaries"), s);
}

void max_jit_iidc_getfeaturemodelist(t_max_jit_iidc *x, t_symbol *s)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("getfeaturemodelist"), s);
}

void max_jit_iidc_getfeaturemode(t_max_jit_iidc *x, t_symbol *s)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("getfeaturemode"), s);
}

void max_jit_iidc_setfeature(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("setfeature"), s, &argc, argv);
}

void max_jit_iidc_setfeaturemode(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	jit_object_method(max_jit_obex_jitob_get(x), gensym("setfeaturemode"), s, &argc, argv);
}

void max_jit_iidc_setroi(t_max_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	printf("max wrapper | setroi - argc : %d, s : %s\n",argc,s->s_name);
	jit_object_method(max_jit_obex_jitob_get(x), gensym("setroi"), s, &argc, argv);
}

void max_jit_iidc_free(t_max_jit_iidc *x)
{
	jit_object_detach(x->servername,x);
	
	max_jit_mop_free(x);
	jit_object_free(max_jit_obex_jitob_get(x));
	max_jit_obex_free(x);
}

void *max_jit_iidc_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_jit_iidc *x;
	void *o;

	if (x=(t_max_jit_iidc *)max_jit_obex_new(max_jit_iidc_class,gensym("jit_iidc"))) {
		if (o=jit_object_new(gensym("jit_iidc"))) {
			
			max_jit_obex_jitob_set(x,o); 
			max_jit_obex_dumpout_set(x,outlet_new(x,NULL)); 
			max_jit_mop_setup(x); 
			max_jit_mop_inputs(x);
			max_jit_mop_outputs(x);
			
			max_jit_attr_args(x,argc,argv);
			
			//NOTIFY : GENERATING A UNIQUE NAME + ASSOCIATING WITH JIT OBJECT(SERVER)
			x->servername = jit_symbol_unique(); 
			jit_object_method(o,_jit_sym_register,x->servername); //this registers w/ the name
			jit_object_attach(x->servername,x);	//this attaches max object(client) with jit object(server)
			 
			
		} else {
			jit_object_error((t_object *)x,"av.jit.iidc: could not allocate object");
			freeobject((t_object *) x);
			x = NULL;
		}
	}
	return (x);
}
