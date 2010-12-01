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
#include "dc1394.h"

//override the definition
#define JIT_DC1394_ERR_RTN(err,message)								\
do {																\
if ((err>0)||(err<=-DC1394_ERROR_NUM))								\
err=DC1394_INVALID_ERROR_CODE;										\
																	\
if (err!=DC1394_SUCCESS) {											\
jit_object_error((t_object *)x,"IIDC error %d : %s - %s",err ,		\
dc1394_error_get_string(err), message);								\
return err;															\
}																	\
} while (0);

#define JIT_DC1394_POST(err,message)								\
do {																\
if (err!=JIT_ERR_NONE){														\
jit_object_post((t_object *)x,message);								\
printf(message);													\
printf("\n");														\
}																	\
} while (0);

#define JIT_DC1394_ERROR(err,message)								\
if (err!=JIT_ERR_NONE){												\
error(message);														\
printf(message);													\
printf(". Error code : %d", (int) err);								\
printf("\n");														\
}																	

#define JIT_DC1394_ERR_CLN_RTN(err,cleanup,message)					\
do {																\
if ((err>0)||(err<=-DC1394_ERROR_NUM))								\
err=DC1394_INVALID_ERROR_CODE;										\
																	\
if (err!=DC1394_SUCCESS) {											\
jit_object_error((t_object *)x,"IIDC error %d : %s - %s", err,		\
dc1394_error_get_string(err), message);								\
cleanup;															\
return err;															\
}																	\
} while (0);

typedef struct _jit_iidc_flags 
{
	int	camera;
	int capture;
	int transmission;
} t_jit_iidc_flags;

typedef struct _jit_iidc 
{
	t_object	ob;
	t_symbol	feature_table[DC1394_FEATURE_NUM];				// feature lookup table
	t_symbol	video_mode_table[DC1394_VIDEO_MODE_NUM];		// video mode lookup table
	t_symbol	feature_mode_table[DC1394_FEATURE_MODE_NUM];	// feature mode lookup table
	t_symbol	color_coding_table[DC1394_COLOR_CODING_NUM];	// color coding lookup table
	t_symbol	framerate_table[DC1394_FRAMERATE_NUM];			// framerate lookup table
	t_jit_iidc_flags		flags;								// few flags...
	
	// iidc attributes
	dc1394camera_t			*camera;			// camera
	t_symbol				cameraguid;			// guid of the current camera

	dc1394camera_list_t		*list;				// list of connected cameras
	
	dc1394featureset_t		features;			// list of features
    dc1394framerates_t		framerates;			// list of supported framerates
    
	t_atom					featurelist[DC1394_FEATURE_NUM];
	long					featurelist_size;
	
	t_symbol				framerate_sym;		// name of the current framerate
    t_symbol				video_mode_sym;		// name of the current video mode
	t_symbol				color_coding_sym;	// name of the current color coding
	
	dc1394framerate_t		framerate_id;		// current framerate
	dc1394video_mode_t		video_mode_id;		// id of the current video mode
    dc1394color_coding_t	color_coding_id;	// id of current color coding
	dc1394speed_t			iso_speed;			// iso speed
	
	int						verbose;			// verbose flag for debugging
	uint64_t				prevstamp;
	int						ring_buffer_size;	// size (in frames) of the ring buffer

} t_jit_iidc;

void *_jit_iidc_class;
static dc1394_t *dc1394_context;

// prototypes
t_jit_err jit_iidc_init(void); 
void jit_iidc_table_init(t_jit_iidc *x);
t_jit_iidc *jit_iidc_new(void);
void jit_iidc_free(t_jit_iidc *x);
t_jit_err jit_iidc_matrix_calc(t_jit_iidc *x, void *inputs, void *outputs);

void cleanup_and_exit(t_jit_iidc *x);
dc1394error_t jit_iidc_open(t_jit_iidc *x);						// open a device and start transmission
void jit_iidc_close(t_jit_iidc *x);								// stop transmission & close the device
t_jit_err jit_iidc_resetfwbus(t_jit_iidc *x);					// reset the firewire bus
dc1394error_t jit_iidc_getbestsettings(t_jit_iidc *x);			// get best video_mode & framerate
dc1394error_t jit_iidc_setup_capture(t_jit_iidc *x);			// setup capture

//get methods
t_jit_err		jit_iidc_getcameraguid(t_jit_iidc *x, void *attr, long *ac, t_atom **av);			// get the guid of the current camera
t_jit_err		jit_iidc_getcameraname(t_jit_iidc *x, void *attr, long *ac, t_atom **av);			// get the name of the current camera
t_jit_err		jit_iidc_getvideomode(t_jit_iidc *x, void *attr, long *ac, t_atom **av);			// get the current video mode
t_jit_err		jit_iidc_getframerate(t_jit_iidc *x, void *attr, long *ac, t_atom **av);			// get the current framerate (not available for formats 6 & 7)
t_jit_err		jit_iidc_getcolorcoding(t_jit_iidc *x, void *attr, long *ac, t_atom **av);			// get the current color coding
dc1394error_t	jit_iidc_getcameraguidlist(t_jit_iidc *x);											// get the list of connected cameras
dc1394error_t	jit_iidc_getvideomodelist(t_jit_iidc *x);											// get the list od supported video modes
dc1394error_t	jit_iidc_getfeaturelist(t_jit_iidc *x);												// get the list of available features
dc1394error_t	jit_iidc_getframeratelist(t_jit_iidc *x);											// get the list of available features for current video mode
dc1394error_t	jit_iidc_getmodel(t_jit_iidc *x);													// get the name of the current camera
dc1394error_t	jit_iidc_getfeature(t_jit_iidc *x, t_symbol *s);									// get the value of a feature
dc1394error_t	jit_iidc_getboundaries(t_jit_iidc *x, t_symbol *s);									// get the features min & max
dc1394error_t	jit_iidc_getfeaturemodelist(t_jit_iidc *x, t_symbol *s);							// get available modes for the selected feature
dc1394error_t	jit_iidc_getfeaturemode(t_jit_iidc *x, t_symbol *s);								// get current mode for the selected feature

//set methods (attributes setter)
t_jit_err		jit_iidc_setcameraguid(t_jit_iidc *x, void *attr, long ac, t_atom *av);				// choose a camera with its guid
t_jit_err		jit_iidc_setcameraname(t_jit_iidc *x, void *attr, long ac, t_atom *av);				// choose a camera with its name
t_jit_err		jit_iidc_setvideomode(t_jit_iidc *x, void *attr, long ac, t_atom *av);				// set video mode
t_jit_err		jit_iidc_setframerate(t_jit_iidc *x, void *attr, long ac, t_atom *av);				// set framerate
t_jit_err		jit_iidc_setcolorcoding(t_jit_iidc *x, void *attr, long ac, t_atom *av);			// set color coding mode
//other
t_jit_err		jit_iidc_setfeature(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);			// set a feature's value
t_jit_err		jit_iidc_setfeaturemode(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);		// set a feature's mode
t_jit_err		jit_iidc_setroi(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv);				// set ROI for format7

// readback functions
dc1394feature_t			jit_iidc_getfeatureid(t_jit_iidc *x, t_symbol *s);							// get the feature's id
dc1394feature_t			jit_iidc_getvideomodeid(t_jit_iidc *x, t_symbol *s);						// get the id of the videomode s
dc1394feature_t			jit_iidc_getfeaturemodeid(t_jit_iidc *x, t_symbol *s);						// get the id of the feature mode s
dc1394color_coding_t	jit_iidc_getcolorcodingid(t_jit_iidc *x, t_symbol *s);						// get the id of the color coding s
dc1394framerate_t		jit_iidc_getframerateid(t_jit_iidc *x, t_symbol *s);						// get the id of the framerate s


t_jit_err jit_iidc_init(void) 
{
	long attrflags=0;
	void *attr,*mop;
	
 	_jit_iidc_class = jit_class_new("jit_iidc",(method)jit_iidc_new,(method)jit_iidc_free,
		sizeof(t_jit_iidc),0L); 

	//add mop
	mop = jit_object_new(_jit_sym_jit_mop,0,1);
	
	
	//disable output linking
	jit_mop_output_nolink(mop,0);

	jit_class_addadornment(_jit_iidc_class,mop);
	
	//add methods
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_matrix_calc,			"matrix_calc",			A_CANT,		0L);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_open,					"open",					A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_close,				"close",				A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_resetfwbus,			"reset",				A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getbestsettings,		"autoset",				A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getcameraguidlist,	"getcameraguidlist",	A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getvideomodelist,		"getvideomodelist",		A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getframeratelist,		"getframeratelist",		A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getmodel,				"getmodel",				A_NOTHING);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getfeature,			"getfeature",			A_SYM);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getboundaries,		"getboundaries",		A_SYM);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getfeaturemodelist,	"getfeaturemodelist",	A_SYM);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_getfeaturemode,		"getfeaturemode",		A_SYM);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_setfeature,			"setfeature",			A_GIMME,	0L);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_setfeaturemode,		"setfeaturemode",		A_GIMME,	0L);
	jit_class_addmethod(_jit_iidc_class, (method)jit_iidc_setroi,				"setroi",				A_GIMME,	0L);
	jit_class_addmethod(_jit_iidc_class, (method)jit_object_register,			"register",				A_CANT,		0L); 
	
	//add attributes
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "cameraguid", _jit_sym_symbol,attrflags,
						  (method)jit_iidc_getcameraguid,
						  (method)jit_iidc_setcameraguid,
						  calcoffset(t_jit_iidc,cameraguid));
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "videomode", _jit_sym_symbol,attrflags,
						  (method)jit_iidc_getvideomode,
						  (method)jit_iidc_setvideomode,
						  calcoffset(t_jit_iidc,video_mode_sym));
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "framerate", _jit_sym_symbol,attrflags,
						  (method)jit_iidc_getframerate,
						  (method)jit_iidc_setframerate,
						  calcoffset(t_jit_iidc,framerate_sym));
	//jit_attr_addfilterset_clip(attr,0,DC1394_FRAMERATE_NUM-1,TRUE,TRUE);	//clip between 0 and the nb of framerate minus 1
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "colorcoding", _jit_sym_long,attrflags,
						  (method)jit_iidc_getcolorcoding,
						  (method)jit_iidc_setcolorcoding,
						  calcoffset(t_jit_iidc,color_coding_sym));
	jit_attr_addfilterset_clip(attr,0,DC1394_FRAMERATE_NUM-1,TRUE,TRUE);	//clip between 0 and the nb of framerate minus 1
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "isospeed", _jit_sym_long,attrflags,
						  (method)0L,
						  (method)0L,
						  calcoffset(t_jit_iidc,iso_speed));
	jit_attr_addfilterset_clip(attr,0,DC1394_ISO_SPEED_NUM-1,TRUE,TRUE);	//clip between 0 and the nb of iso speed minus 1
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "ring_buffer_size", _jit_sym_long,attrflags,
						  (method)0L,
						  (method)0L,
						  calcoffset(t_jit_iidc,ring_buffer_size));
	jit_attr_addfilterset_clip(attr,1,32,TRUE,TRUE);	//clip between 0 and the nb of iso speed minus 1
	jit_class_addattr(_jit_iidc_class,attr);
	
	attr = jit_object_new(_jit_sym_jit_attr_offset, "verbose", _jit_sym_long,attrflags,
						  (method)0L,
						  (method)0L,
						  calcoffset(t_jit_iidc,verbose));
	jit_attr_addfilterset_clip(attr,0,1,TRUE,TRUE);							//clip between 0 and 1
	jit_class_addattr(_jit_iidc_class,attr);
	
	attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_OPAQUE_USER;
	attr = jit_object_new(_jit_sym_jit_attr_offset_array, "featurelist", _jit_sym_atom, DC1394_FEATURE_NUM, attrflags, 
						  (method)0L, 
						  (method)0L, 
						  calcoffset(t_jit_iidc,featurelist_size),
						  calcoffset(t_jit_iidc,featurelist));
	jit_class_addattr(_jit_iidc_class,attr);
	
	jit_class_register(_jit_iidc_class);
	
	dc1394_context = dc1394_new ();									// initialize libdc1394
	if (!dc1394_context)
	{
		error("libdc1394 : initialisation error");
		return JIT_ERR_GENERIC;
	}
	
	post("av.jit.iidc by Antoine Villeret, build on %s at %s.",__DATE__,__TIME__);

	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_matrix_calc(t_jit_iidc *x, void *inputs, void *outputs)
{
	t_jit_err err=JIT_ERR_NONE;
	dc1394error_t iidc_err=DC1394_SUCCESS;
	long out_savelock = 0;
	t_jit_matrix_info out_minfo;
	uchar *out_bp;
	void *out_matrix;
	dc1394video_frame_t *frame;
	float period;

	// if no grabber is currently open, don't do anything
	if(!x->flags.transmission)
	{
		return 0;
	}
	
	//Get the image from the camera
	JIT_DC1394_POST(x->verbose,"grab a frame");
	iidc_err=dc1394_capture_dequeue(x->camera, DC1394_CAPTURE_POLICY_WAIT, &frame);
    DC1394_ERR_CLN_RTN(iidc_err,jit_iidc_close(x),"Could not capture a frame");
	// compute the real framerate
	period = (frame->timestamp - x->prevstamp) / 1000.;
	x->prevstamp = frame->timestamp;
	//printf("frame period : %f ms\n", period);
	
	if (x->verbose) {
		// post in max
		post("-------------FRAME INFO-------------");
		post("video mode : %d", frame->video_mode);
		post("image size : %d %d",frame->size[0],frame->size[1]);
		post("position : %d,%d", frame->position[0],frame->position[1]);
		post("color coding : %d, color filter : %d", frame->color_coding-DC1394_COLOR_CODING_MIN, frame->color_filter-DC1394_COLOR_FILTER_MIN);
		post("data depth : %d bit", frame->data_depth);
		post("stride : %d",frame->stride);
		post("total byte/stride : %d",frame->total_bytes/frame->stride);
		post("padding byte : %d", frame->padding_bytes);
		post("packet size : %d bytes, packets per frame : %d", frame->packet_size, frame->packets_per_frame);
		post("frame behind : %d", frame->frames_behind);
		post("byte per pixel : %d", frame->data_depth);
		post("-----------------------------------");
		
		// print in stdout
		printf("-------------FRAME INFO-------------\n");
		printf("video mode : %d\n", frame->video_mode);
		printf("image size : %d %d\n",frame->size[0],frame->size[1]);
		printf("position : %d,%d\n", frame->position[0],frame->position[1]);
		printf("color coding : %d, color filter : %d\n", frame->color_coding-DC1394_COLOR_CODING_MIN, frame->color_filter-DC1394_COLOR_FILTER_MIN);
		printf("data depth : %d bit\n", frame->data_depth);
		printf("stride : %d\n",frame->stride);
		printf("total byte/stride : %lld\n",frame->total_bytes/frame->stride);
		printf("padding byte : %d\n", frame->padding_bytes);
		printf("packet size : %d bytes, packets per frame : %d\n", frame->packet_size, frame->packets_per_frame);
		printf("frame behind : %d\n", frame->frames_behind);
		printf("byte per pixel : %d\n", frame->data_depth);
		printf("-----------------------------------\n");
		
	}
	
	if(x->verbose && dc1394_capture_is_frame_corrupt(x->camera, frame)) jit_object_error((t_object *)x, "Corrupted frame");
	
	//Get pointers to matrices
	out_matrix 	= jit_object_method(outputs,_jit_sym_getindex,0);

	if (x&&out_matrix) 
	{
		//Lock the matrice
		out_savelock = (long) jit_object_method(out_matrix,_jit_sym_lock,1);
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
		
	} else {
		err = JIT_ERR_INVALID_PTR;
		goto out;
	}
	
	// prepare output
	out_minfo.dimcount = 2;
	out_minfo.dim[0] = frame->size[0];
	out_minfo.dim[1] = frame->size[1];
	
	
	if ( frame->color_coding >= DC1394_COLOR_CODING_YUV411 && frame->color_coding <= DC1394_COLOR_CODING_YUV444 ){
		
		//dc1394video_frame_t *RGB_frame=calloc(1,sizeof(dc1394video_frame_t));
		dc1394video_frame_t RGB_frame;
		out_minfo.planecount = 3;
		out_minfo.type = _jit_sym_char;
		
		jit_object_method(out_matrix,_jit_sym_setinfo,&out_minfo);		// store matrixinfo
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);		// update matrix info
		
		//convert color coding
		//RGB_frame->color_coding=DC1394_COLOR_CODING_RGB8;
		RGB_frame.color_coding=DC1394_COLOR_CODING_RGB8;
		//dc1394_convert_frames(frame, RGB_frame);
		dc1394_convert_frames(frame, &RGB_frame);
		
		//Get the data pointer
		jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);
		if (!out_bp) {err=JIT_ERR_INVALID_OUTPUT; goto out;}
		
		//Copy frame from the camera
		//memcpy(out_bp,RGB_frame->image,RGB_frame->total_bytes);
		memcpy(out_bp,RGB_frame.image,RGB_frame.total_bytes);
		
		//free RGB_frame
		//free(RGB_frame->image);
		//free(RGB_frame);
	}
	else {
		// dimstride is always rounded to the next multiple of 16
		// so it might be some problem when the image width isn't divisible by 16
		// for example in full size the size could be something like 660 x 492
		
		if (frame->color_coding == DC1394_COLOR_CODING_MONO8) {
			out_minfo.planecount = sizeof(char);
			out_minfo.type = _jit_sym_char;
			out_minfo.dimstride[0] = 1;
			out_minfo.dimstride[1] = frame->stride;
		}
		else if (frame->color_coding == DC1394_COLOR_CODING_MONO16){
			out_minfo.planecount = 2*sizeof(char);
			out_minfo.type = _jit_sym_char;
			out_minfo.dimstride[0] = 1;
			out_minfo.dimstride[1] = frame->stride;
		}
		else if (frame->color_coding == DC1394_COLOR_CODING_RGB16){
			out_minfo.planecount = 6*sizeof(char);
			out_minfo.type = _jit_sym_char;
			out_minfo.dimstride[0] = 3;
			out_minfo.dimstride[1] = frame->stride;
		}
		else if (frame->color_coding == DC1394_COLOR_CODING_RGB8){
			out_minfo.planecount = 3*sizeof(char);
			out_minfo.type = _jit_sym_char;
			out_minfo.dimstride[0] = 3;
			out_minfo.dimstride[1] = frame->stride;
		}
		else {
			JIT_DC1394_ERR_RTN(err,"Color mode isn't supported");
		}
		
		jit_object_method(out_matrix,_jit_sym_setinfo,&out_minfo);		// store matrixinfo
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);		// update matrix info
		
		//Get the data pointer
		jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);
		if (!out_bp) {err=JIT_ERR_INVALID_OUTPUT; goto out;}
		
		//Copy frame from the camera
		memcpy(out_bp,frame->image,frame->total_bytes);
	}
	
	if (x->verbose) {
		
		// post in max console
		jit_object_post((t_object *)x, "---------MATRIX INFO---------");
		jit_object_post((t_object *)x, "size : %ld bytes", out_minfo.size);
		jit_object_post((t_object *)x, "type : %s", out_minfo.type->s_name);
		jit_object_post((t_object *)x, "flag : %ld", out_minfo.flags);
		jit_object_post((t_object *)x, "dimcount : %ld", out_minfo.dimcount);
		jit_object_post((t_object *)x, "dim : %ld x %ld", out_minfo.dim[0],out_minfo.dim[1]);
		jit_object_post((t_object *)x, "dimstride : %ld, %ld", out_minfo.dimstride[0],out_minfo.dimstride[1]);
		jit_object_post((t_object *)x, "planecount : %ld", out_minfo.planecount);
		jit_object_post((t_object *)x, "-----------------------------");
		jit_object_post((t_object *)x, "total frame byte : %lld, total matrix byte : %ld",frame->total_bytes,out_minfo.size);
		
		// print in stdout
		printf("---------MATRIX INFO---------\n");
		printf("size : %ld bytes\n", out_minfo.size);
		printf("type : %s\n", out_minfo.type->s_name);
		printf("flag : %ld\n", out_minfo.flags);
		printf("dimcount : %ld\n", out_minfo.dimcount);
		printf("dim : %ld x %ld\n", out_minfo.dim[0],out_minfo.dim[1]);
		printf("dimstride : %ld, %ld\n", out_minfo.dimstride[0],out_minfo.dimstride[1]);
		printf("planecount : %ld\n", out_minfo.planecount);
		printf("-----------------------------\n");
		printf("total frame byte : %lld, total matrix byte : %ld\n\n",frame->total_bytes,out_minfo.size);
		
	}
	
out:
	//Release the frame buffer
	iidc_err=dc1394_capture_enqueue(x->camera, frame);
	if(iidc_err!=DC1394_SUCCESS) {
		jit_object_error((t_object *)x, "Can't release the frame buffer, Max may crash...");
		printf("Can't release the frame buffer, Max may crash...");
	}
	
	jit_object_method(out_matrix,_jit_sym_lock,out_savelock);
	if(x->verbose) printf("end of matrix_calc method\n");
	return err;
}

t_jit_iidc *jit_iidc_new(void)
{
	t_jit_iidc *x;
		
	if (x=(t_jit_iidc *)jit_object_alloc(_jit_iidc_class)) {
		// initialization
		
		x->framerate_id	= DC1394_FRAMERATE_MIN - 1;
		x->camera = NULL;
		x->flags.camera=0;
		x->flags.capture=0;
		x->flags.transmission=0;
		x->cameraguid=*gensym("NULL");
		x->prevstamp = 0;
		x->ring_buffer_size = 4;
		
		jit_iidc_table_init(x);
	} else {
		x = NULL;
	}
	return x;
}

void jit_iidc_free(t_jit_iidc *x)
{
	jit_iidc_close(x);
	//dc1394_free(x->d);
}

dc1394error_t  jit_iidc_open(t_jit_iidc *x)
{
	dc1394error_t err;
	dc1394camera_list_t		*list;				// list of connected cameras

	// exit with error message if a grabber is already sending data
	if (x->flags.transmission)
	{
		jit_object_error((t_object *)x, "A grabber is already open");
		return DC1394_FAILURE;
	}
	JIT_DC1394_POST(x->verbose,"opening a device");

	// if there is no camera try to get the first one
	if(!x->flags.camera){
		
		JIT_DC1394_POST(x->verbose,"enumerating cameras");
		//enumerate cameras...
		err=dc1394_camera_enumerate (dc1394_context, &list);
		JIT_DC1394_ERR_RTN(err,"Failed to enumerate cameras");
		
		if (list->num == 0) 
		{
			jit_object_error((t_object *)x, "No cameras found");
			return 1;
		}
		
		//... and select the first one
		x->camera = dc1394_camera_new (dc1394_context, list->ids[0].guid);
		if (!x->camera) 
		{
			jit_object_error((t_object *)x, "Failed to initialize camera with guid %.16llX", list->ids[0].guid);
			return 1;
		}
		else {
			x->flags.camera = 1;
		}
		dc1394_camera_free_list (list);
	}
	
	err = dc1394_camera_print_info (x->camera, stdout);
	if (err) printf("can't print camera infos");
	
	// select B-mode if available
	if (x->camera->bmode_capable)
	{
		x->iso_speed=DC1394_ISO_SPEED_800;
		JIT_DC1394_POST(x->verbose,"ISO speed 800 Mbps");
		err=dc1394_video_set_operation_mode(x->camera, DC1394_OPERATION_MODE_1394B);
		JIT_DC1394_ERR_RTN(err,"Failed to set operation mode to 1394b.");
	}
	else {
		x->iso_speed=DC1394_ISO_SPEED_400;
		JIT_DC1394_POST(x->verbose,"ISO speed 400 Mbps");
		err=dc1394_video_set_operation_mode(x->camera, DC1394_OPERATION_MODE_LEGACY);
		JIT_DC1394_ERR_RTN(err,"Failed to set operation mode to LEGACY.");
	}
	
	
	//if format or mode have not been specified, try to use the best...
	if(x->video_mode_id < DC1394_VIDEO_MODE_MIN || x->video_mode_id > DC1394_VIDEO_MODE_MAX ||
	   x->framerate_id < DC1394_FRAMERATE_MIN || x->framerate_id >  DC1394_FRAMERATE_MAX)
	{
		err=jit_iidc_getbestsettings(x);
		JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Can't get best settings !");
	}

	// setup capture
	err=jit_iidc_setup_capture(x);
	if (err!=DC1394_SUCCESS) {
        jit_object_error((t_object *)x,"Could not setup capture");
		return 0;
    }
	
    /*-----------------------------------------------------------------------
     *  report camera's features
     *-----------------------------------------------------------------------*/
    
	
	err=dc1394_feature_get_all(x->camera,&x->features);
    if (err!=DC1394_SUCCESS) {
        jit_object_error((t_object *)x,"Could not get feature set");
		if(x->verbose) dc1394_feature_print_all(&x->features, stdout);
    }
	jit_iidc_getfeaturelist(x);
	
	
	/*-----------------------------------------------------------------------
     *  have the camera start sending us data
     *-----------------------------------------------------------------------*/
	dc1394switch_t pwr;
	
	JIT_DC1394_POST(x->verbose,"set transmission");
	err=dc1394_video_set_transmission(x->camera, DC1394_ON);
    JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not start camera iso transmission");
	x->flags.transmission=1;
	err=dc1394_video_get_mode(x->camera, &x->video_mode_id);
	
	dc1394_video_get_transmission(x->camera, &pwr);
	printf("transmission : %s\n", pwr?"ON":"OFF");

	return err;
}

void jit_iidc_close(t_jit_iidc *x)
{
	// stop transmission
	if(x->flags.transmission){
		dc1394_video_set_transmission(x->camera, DC1394_OFF);
		x->flags.transmission=0;
	}
	
	// stop capture
	if(x->flags.capture){
		dc1394_capture_stop(x->camera);
		x->flags.capture=0;
	}
	
	// close camera
	if (x->flags.camera) {
		dc1394_camera_free(x->camera);
		x->flags.camera=0;
	}
}

t_jit_err jit_iidc_resetfwbus(t_jit_iidc *x){
	
	dc1394error_t err;
	int trans=0;
	
	if (!x->flags.camera)
	{
		printf("camera must be open to reset the bus");
		jit_object_error((t_object *)x, "camera must be open to reset the bus");
		return 0;
	}
	
	// stop transmission & capture
	if(x->flags.transmission){
		dc1394_video_set_transmission(x->camera, DC1394_OFF);
		x->flags.transmission=0;
		dc1394_capture_stop(x->camera);
		x->flags.capture=0;
		trans=1;
	}
	
	err=dc1394_reset_bus(x->camera);
	if(err){
		jit_object_error((t_object *)x, "can't reset fw bus");
		printf("can't reset the fw bus");
		return JIT_ERR_GENERIC;
	}
	
	if (trans) {
		jit_iidc_open(x);
	}
	
	return JIT_ERR_NONE;
}


dc1394error_t jit_iidc_setup_capture(t_jit_iidc *x)
{
	dc1394error_t err;
	/*-----------------------------------------------------------------------
     *  setup capture
     *-----------------------------------------------------------------------*/
	
	// set iso speed
	if(x->verbose) printf("set iso speed\n");
    err=dc1394_video_set_iso_speed(x->camera, x->iso_speed);
	/*if(err) {
		err=dc1394_video_set_iso_speed(x->camera, DC1394_ISO_SPEED_400);
	}
	 */
    JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not set iso speed");
	
	// set video mode
	if(x->verbose) printf("set video mode\n");
    err=dc1394_video_set_mode(x->camera, x->video_mode_id);
    JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not set video mode");
	
	/*
	// set the ROI if needed
	if(x->verbose) printf("set ROI");
	if(dc1394_is_video_mode_scalable(x->video_mode_id)){
		
	}
	*/
	   
	// set framerate
	if(x->verbose) printf("set framerate\n");
    err=dc1394_video_set_framerate(x->camera, x->framerate_id);
    JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not set framerate");
	
	// set color coding from video mode
	if(x->verbose) printf("set color coding from video mode\n");
	err=dc1394_get_color_coding_from_video_mode(x->camera, x->video_mode_id,&x->color_coding_id);
	JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not get color coding");
	
	// capture setup
	if(x->verbose) printf("set capture settings\n");
    err=dc1394_capture_setup(x->camera,x->ring_buffer_size, DC1394_CAPTURE_FLAGS_DEFAULT);
    JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not setup capture,make sure that the video mode and the framerate are supported.");
	
	if(x->verbose) printf("capture setup well done\n");
	x->flags.capture=1;
	
	return DC1394_SUCCESS;
}

t_jit_err jit_iidc_getcameraguid(t_jit_iidc *x, void *attr, long *ac, t_atom **av)
{	
	if (x->camera == NULL) {
		jit_object_error((t_object *)x, "Camera not initialized");
		return JIT_ERR_GENERIC;
	}
	
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	//jit_object_post((t_jit_object *)x,"video mode : %s", x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name);
	sprintf(x->cameraguid.s_name,"%.16llX",x->camera->guid);
	jit_atom_setsym(*av,&x->cameraguid);
	
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_getcameraname(t_jit_iidc *x, void *attr, long *ac, t_atom **av)
{
	printf("getcameraname message received");
	
	if (x->camera == NULL) {
		jit_object_error((t_object *)x, "Camera not initialized");
		return JIT_ERR_GENERIC;
	}
	
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	//jit_object_post((t_jit_object *)x,"video mode : %s", x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name);
	//printf("name : %s",x->camera->model);
	//sprintf(x->cameraguid.s_name,"%.16llX",x->camera->guid);
	//jit_atom_setsym(*av,&x->camera_name);
	
	return JIT_ERR_NONE;
}


t_jit_err jit_iidc_getvideomode(t_jit_iidc *x, void *attr, long *ac, t_atom **av)
{
	dc1394error_t err;
	dc1394video_mode_t video_mode;
	
	if (x->camera == NULL) {
		jit_object_error((t_object *)x, "Camera not initialized\n");
		return JIT_ERR_GENERIC;
	}
	
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	
	err=dc1394_video_get_mode(x->camera, &video_mode);
	JIT_DC1394_ERR_RTN(err,"Can't get videomode\n");
	printf("videomode : %d\n",video_mode);
	
	x->video_mode_id=video_mode;
	x->video_mode_sym = x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN];
	jit_atom_setsym(*av,&x->video_mode_sym);
	
	return JIT_ERR_NONE;
}

t_jit_err	jit_iidc_getframerate(t_jit_iidc *x,  void *attr, long *ac, t_atom **av)
{
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	x->framerate_sym=x->framerate_table[x->framerate_id-DC1394_FRAMERATE_MIN];
	jit_atom_setsym(*av,&x->framerate_sym);
	
	return JIT_ERR_NONE;
}

t_jit_err	jit_iidc_getcolorcoding(t_jit_iidc *x, void *attr, long *ac, t_atom **av)
{
	dc1394error_t err;
	
	if (x->camera == NULL) {
		jit_object_error((t_object *)x, "Camera not initialized");
		return JIT_ERR_GENERIC;
	}
	
	if ((*ac)&&(*av)) {
		//memory passed in, use it
	} else {
		//otherwise allocate memory
		*ac = 1;
		if (!(*av = jit_getbytes(sizeof(t_atom)*(*ac)))) {
			*ac = 0;
			return JIT_ERR_OUT_OF_MEM;
		}
	}
	
	err=dc1394_get_color_coding_from_video_mode(x->camera,x->video_mode_id, &x->color_coding_id);
	if(err)
	{
		printf("can't get color coding from video mode, error : %d",err);
		jit_object_error((t_object *)x, "can't get color coding from video mode, error : %d",err);
		return JIT_ERR_NONE;
	}
	//jit_object_post((t_jit_object *)x,"video mode : %s", x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name);
	x->color_coding_sym = x->color_coding_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN];
	jit_atom_setsym(*av,&x->video_mode_sym);
	
	return JIT_ERR_NONE;
}

dc1394error_t jit_iidc_getcameraguidlist(t_jit_iidc *x)
{
	int						i;
	dc1394error_t			err;
	dc1394camera_list_t		*list;				// list of connected cameras
	t_atom					info[64];			// maximum of 64 cameras per fw bus
	char					guid[17];
	
	printf("getcamera list\n");
	
	//enumerate cameras...
    err=dc1394_camera_enumerate (dc1394_context, &list);
    JIT_DC1394_ERR_RTN(err,"Failed to enumerate cameras");

	
    if (list->num == 0) 
	{
        jit_object_error((t_object *)x, "No cameras found");
        return 1;
    }
	
	//... and print some infos
	for (i=0; i<list->num; i++) 
	{
		sprintf(guid,"%.16llX",list->ids[i].guid);
		jit_atom_setsym(&info[i+1],gensym(guid));
		post("Camera with GUID %.16llX\n", list->ids[i].guid);
	}
	
	jit_atom_setlong(&info[0], i);
	
	jit_object_notify(x,gensym("cameraguidlist"), info);
	
	dc1394_camera_free_list (list);
	
	return err;
	
}

dc1394error_t jit_iidc_getvideomodelist(t_jit_iidc *x)
{
	int i,id;
	t_atom info[DC1394_VIDEO_MODE_NUM];
	dc1394video_modes_t video_modes;			// list of supported videomodes
	dc1394error_t err=DC1394_SUCCESS;
	
	if(!x->flags.camera){
		jit_object_error((t_object *)x, "open a camera before trying to get its video mode");
		return 0;
	}
	
	err=dc1394_video_get_supported_modes(x->camera,&video_modes);
	JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Can't get video modes");
	
	for (i=0; i<video_modes.num ; i++) 
	{
		id = video_modes.modes[i]-DC1394_VIDEO_MODE_MIN;
		jit_atom_setsym(&info[i+1],gensym(x->video_mode_table[id].s_name));
	}
	jit_atom_setlong(&info[0], i);
		
	jit_object_notify(x,gensym("videomodelist"), info);	
	
	return err;
	
}

dc1394error_t jit_iidc_getfeaturelist(t_jit_iidc *x)
{
	int i,count=0;
	for (i=0; i<DC1394_FEATURE_NUM; i++) {
		if(x->features.feature[i].available)
		{
			jit_atom_setsym(x->featurelist+count, gensym(x->feature_table[i].s_name));
			count++;				// increment position in featurelist
		}
	}
	printf("count : %d\n",count);
	x->featurelist_size = count;
	return DC1394_SUCCESS;
}

dc1394error_t jit_iidc_getframeratelist(t_jit_iidc *x)
{
	int i=0;
	t_atom				info[DC1394_FEATURE_NUM];
	dc1394framerates_t	framerate_list;
	dc1394framerate_t	framerate;
	dc1394error_t		err;
	
	err = dc1394_video_get_supported_framerates(x->camera, x->video_mode_id, &framerate_list);
	if(err!=DC1394_SUCCESS){
		jit_object_error((t_object *)x, "Can't get available framerates list.");
		return err;
	}
	
	for (i=0; i<framerate_list.num; i++) {
		framerate = framerate_list.framerates[i];
		jit_atom_setsym(&info[i+1],gensym(x->framerate_table[framerate-DC1394_FRAMERATE_MIN].s_name));
	}
	
	jit_atom_setlong(&info[0], i);
	
	jit_object_notify(x,gensym("frameratelist"), info);
	
	return DC1394_SUCCESS;
}

dc1394error_t jit_iidc_getmodel(t_jit_iidc *x)
{
	t_atom			info[2];
	
	if (!x->flags.transmission){
		jit_object_error((t_object *)x, "Open a device before trying to get its model...");
	}
	else {
		jit_atom_setsym(&info[0], gensym(x->camera->vendor));
		jit_atom_setsym(&info[1], gensym(x->camera->model));
		
		jit_object_notify(x,gensym("model"), &info);
	}

	return DC1394_SUCCESS;
	
}

dc1394error_t jit_iidc_getfeature(t_jit_iidc *x, t_symbol *s)
{
	dc1394error_t err;
	dc1394feature_t id;
	uint32_t value = -1;
	t_atom info[2];
	
	id = jit_iidc_getfeatureid(x, s);
	if(!(id>=DC1394_FEATURE_MIN||id<=DC1394_FEATURE_MAX)) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}
	if (!x->features.feature[id-DC1394_FEATURE_MIN].available) {
		jit_object_error((t_object *)x,"This feature is not available on this camera");
		return DC1394_FAILURE;
	}
	err=dc1394_feature_get_value(x->camera, id, &value);
	JIT_DC1394_ERR_RTN(err,"Failed to get the value");
	
	jit_atom_setsym(&info[0],s);
	jit_atom_setlong(&info[1],value);
	jit_object_notify(x,gensym("feature"), info);
	
	return DC1394_SUCCESS;
}

dc1394error_t jit_iidc_getboundaries(t_jit_iidc *x, t_symbol *s)
{
	dc1394error_t err;
	dc1394feature_t id;
	uint32_t min,max;
	t_atom info[3];
	
	id = jit_iidc_getfeatureid(x, s);
	if(id==DC1394_FEATURE_MIN-1) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}
	if (!x->features.feature[id-DC1394_FEATURE_MIN].available) {
		jit_object_error((t_object *)x,"This feature is not available on this camera");
		return DC1394_FAILURE;
	}
	err=dc1394_feature_get_boundaries(x->camera, id, &min, &max);
	JIT_DC1394_ERR_RTN(err,"Failed to set the value.");
	
	jit_atom_setsym(&info[0],s);
	jit_atom_setlong(&info[1],min);
	jit_atom_setlong(&info[2],max);
	jit_object_notify(x,gensym("boundaries"), info);
	
	return DC1394_SUCCESS;
}

dc1394error_t jit_iidc_getfeaturemodelist(t_jit_iidc *x, t_symbol *s)
{
	int i,mode;
	t_atom info[DC1394_FEATURE_MODE_NUM+1];
	dc1394feature_t id;
	
	id = jit_iidc_getfeatureid(x, s);
	if(id==DC1394_FEATURE_MIN-1) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}
	
	id -= DC1394_FEATURE_MIN;
	
	if (!x->features.feature[id].available) {
		jit_object_error((t_object *)x,"This feature is not available on this camera");
		return DC1394_FAILURE;
	}

	for (i=0; i<x->features.feature[id].modes.num; i++) {
		
		mode=x->features.feature[id].modes.modes[i]-DC1394_FEATURE_MODE_MIN;
		jit_atom_setsym(&info[i+1],gensym(x->feature_mode_table[mode].s_name));
		
	}
	
	jit_atom_setlong(&info[0], x->features.feature[id].modes.num);
	
	jit_object_notify(x,gensym("featuremodelist"), info);
	
	return DC1394_SUCCESS;
}

dc1394error_t	jit_iidc_getfeaturemode(t_jit_iidc *x, t_symbol *s)
{
	dc1394error_t err;
	dc1394feature_mode_t mode;
	dc1394feature_t id;
	t_atom info[1];
	
	id = jit_iidc_getfeatureid(x, s);
	if(id==DC1394_FEATURE_MIN-1) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}

	err = dc1394_feature_get_mode(x->camera, id, &mode);
	if (err)
	{
		jit_object_error((t_object *)x,"Can't get feature mode");
		return DC1394_FAILURE;
	}
	
	jit_atom_setsym(&info[0],&x->feature_mode_table[mode-DC1394_FEATURE_MODE_MIN]);
	
	jit_object_notify(x,gensym("featuremode"), info);
	
	return DC1394_SUCCESS;
}

t_jit_err jit_iidc_setcameraguid(t_jit_iidc *x, void *attr, long ac, t_atom *av)
{
	uint64_t guid = 0;
	int wasopen=0;
	
	if (ac!=1) 
	{
		jit_object_error((t_object *)x, "cameraguid message needs one argument (symbol)");
	}
	else
	{
		if (atom_gettype(av)!=A_SYM) {
			jit_object_error((t_object *)x, "video_mode message needs a symbol as argument");
		}
		else {
			printf("Using camera with guid : %s\n",jit_atom_getsym(av)->s_name);
			sscanf(jit_atom_getsym(av)->s_name, "%llX", &guid);					// convert symbol from t_atom *av to long long int
			if(x->flags.transmission) {
				printf("close the camera before changing it\n");
				wasopen = 1;
				jit_iidc_close(x);
			}
			x->camera = dc1394_camera_new (dc1394_context,guid);							// create a new camera based on a GUID
			if (!x->camera) 
			{
				jit_object_error((t_object *)x, "Failed to initialize camera with guid %.16llX", guid);
				return 1;
			}
			else {
				x->flags.camera = 1;
			}
			
			// get best settings for the new camera
			jit_iidc_getbestsettings(x);
			
			if (wasopen) {
				jit_iidc_open(x);
			}
			
			x->cameraguid.s_name = jit_atom_getsym(av)->s_name;				// store the GUID as symbol in the object attribute
			x->video_mode_id = DC1394_VIDEO_MODE_MIN-1;							// reset video_mode
			jit_iidc_getbestsettings(x);										// and get bestsettings
			
		}
	}
	
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_setcameraname(t_jit_iidc *x, void *attr, long ac, t_atom *av)
{
	//nada
	return 0;
}

t_jit_err jit_iidc_setvideomode(t_jit_iidc *x, void *attr, long ac, t_atom *av)
{
	int id = DC1394_VIDEO_MODE_MIN-1;
	t_atom roi[4];
	dc1394error_t err;
	
	printf("set video mode\n");
	if (ac!=1) 
	{
		jit_object_error((t_object *)x, "videomode message needs one argument (symbol)");
	}
	else
	{
		if (atom_gettype(av)!=A_SYM) {
			jit_object_error((t_object *)x, "videomode message needs a symbol as argument");
		}
		else {
			id = jit_iidc_getvideomodeid(x, jit_atom_getsym(av));
			if (id<DC1394_VIDEO_MODE_MIN) {
				jit_object_error((t_object *)x, "there is no videomode called \"%s\"",jit_atom_getsym(av)->s_name);
				
			}
			else {
				x->video_mode_id = id;
				x->video_mode_sym = *gensym(jit_atom_getsym(av)->s_name);
			}
		}
				
		// reset capture
		if(x->flags.transmission){
			printf("reset capture\n");
			dc1394_video_set_transmission(x->camera, DC1394_OFF);
			x->flags.transmission=0;
			if(x->flags.capture){
				dc1394_capture_stop(x->camera);
				x->flags.capture=0;
			}
			err=jit_iidc_setup_capture(x);
			if(err) {
				jit_object_error((t_object *)x, "can't set the capture");
				return JIT_ERR_GENERIC;
			}
			
			dc1394_video_set_transmission(x->camera, DC1394_ON);
			x->flags.transmission=1;
		}
		
		
	}
	printf("scalable mode ?\n");
	if (dc1394_is_video_mode_scalable(x->video_mode_id))
	{
		dc1394color_coding_t color_coding;
		uint32_t cpacket_size, cleft, ctop, cwidth, cheight;
		dc1394format7mode_t f7_mode;
		
		err=dc1394_format7_get_mode_info(x->camera, x->video_mode_id, &f7_mode);
		JIT_DC1394_ERR_RTN(err,"Failed to get video mode infos\n");
		
		// set default values for ROI
		jit_atom_setlong(roi,0);
		jit_atom_setlong(roi+1,0);
		jit_atom_setlong(roi+2,f7_mode.max_size_x);
		jit_atom_setlong(roi+3,f7_mode.max_size_y);
		jit_iidc_setroi(x, gensym("setroi"), 4, roi);
		
		err=dc1394_format7_get_roi(x->camera, x->video_mode_id, &color_coding, &cpacket_size, &cleft, &ctop, &cwidth, &cheight);
		JIT_DC1394_ERR_RTN(err,"can't get ROI from current video mode\n");
		
		printf("new RIO settings : \n");
		printf("colorcoding \t : %d\n",color_coding);
		printf("packet-size \t : %d \n",cpacket_size);
		printf("left \t : %d \n",cleft);
		printf("top \t : %d \n",ctop);
		printf("width \t : %d \n",cwidth);
		printf("height \t : %d \n",cheight);
	}
	printf("video mode successful done\n");
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_setframerate(t_jit_iidc *x, void *attr, long ac, t_atom *av)
{
	dc1394error_t err;
	
	if (ac!=1) 
	{
		jit_object_error((t_object *)x, "Framerate message needs one argument (symbol).");
		return JIT_ERR_GENERIC;
	}
	if (atom_gettype(av)!=A_SYM){
		jit_object_error((t_object *)x,"Framerate must be a symbol.");
		return JIT_ERR_GENERIC;
	}
	else
	{
		x->framerate_id = jit_iidc_getframerateid(x, jit_atom_getsym(av));
		
		if (x->framerate_id<DC1394_FRAMERATE_MIN) {
			printf("invalid framerate\n");
			jit_object_error((t_object *)x, "invalid framerate");
			return JIT_ERR_GENERIC;
		}
		x->framerate_sym=*jit_atom_getsym(av);
		// reset capture
		if(x->flags.transmission){
			dc1394_video_set_transmission(x->camera, DC1394_OFF);
			x->flags.transmission=0;
			if(x->flags.capture){
				dc1394_capture_stop(x->camera);
				x->flags.capture=0;
			}
			err=jit_iidc_setup_capture(x);
			if(err) {
				jit_object_error((t_object *)x, "can't set the capture");
				return JIT_ERR_GENERIC;
			}
			
			dc1394_video_set_transmission(x->camera, DC1394_ON);
			x->flags.transmission=1;
		}
	}
		
	return JIT_ERR_NONE;
} 

t_jit_err		jit_iidc_setcolorcoding(t_jit_iidc *x, void *attr, long ac, t_atom *av)
{
	jit_object_error((t_object *)x, "not implemented yet, sorry...");
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_setfeature(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	dc1394error_t err;
	dc1394feature_t id;
	
	//printf("jit.iidc.c | setfeature - s : %s, argc : %i, argv : %ld\n", s->s_name, argc, jit_atom_getlong(argv));
	
	id = jit_iidc_getfeatureid(x, jit_atom_getsym(argv));
	if(!(id>=DC1394_FEATURE_MIN||id<=DC1394_FEATURE_MAX)) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}
	if (!x->features.feature[id-DC1394_FEATURE_MIN].available) {
		jit_object_error((t_object *)x,"This feature is not available on this camera");
		return DC1394_FAILURE;
	}
	if (x->verbose) post("set feature %s (%d) : %f", jit_atom_getsym(argv)->s_name, id, jit_atom_getfloat(argv+1));
	err=dc1394_feature_set_value(x->camera, id, (uint32_t) jit_atom_getlong(argv+1));
	JIT_DC1394_ERR_RTN(err,"Failed to set the value");
	
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_setfeaturemode(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	dc1394error_t err;
	dc1394feature_t id;
	dc1394feature_mode_t mode;
	
	if (argc !=2 ) {
		jit_object_error((t_object *)x,"%s needs 2 symbol arguments : feature's name & mode");
		return JIT_ERR_GENERIC;
	}
	if (atom_gettype(argv)!=A_SYM && atom_gettype(argv+1)!=A_SYM){
		jit_object_error((t_object *)x,"%s needs 2 symbol arguments : feature's name & mode");
		return JIT_ERR_GENERIC;
	}	
	
	id = jit_iidc_getfeatureid(x, jit_atom_getsym(argv));
	if(!(id>=DC1394_FEATURE_MIN||id<=DC1394_FEATURE_MAX)) 
	{
		jit_object_error((t_object *)x,"This feature doesn't exist at all");
		return DC1394_FAILURE;
	}
	if (!x->features.feature[id-DC1394_FEATURE_MIN].available) {
		jit_object_error((t_object *)x,"This feature is not available on this camera");
		return DC1394_FAILURE;
	}
	
	mode=jit_iidc_getfeaturemodeid(x, jit_atom_getsym(argv+1));
	if (mode == DC1394_FEATURE_MODE_MIN-1)
	{
		jit_object_error((t_object *)x,"This feature mode doesn't exist at all");
		return DC1394_FAILURE;
	}
	
	err=dc1394_feature_set_mode(x->camera, id, mode);
	JIT_DC1394_ERR_RTN(err,"Failed to set the mode");
	
	if (x->verbose) post("set feature mode %s (%d) : %s", jit_atom_getsym(argv)->s_name, id, jit_atom_getsym(argv+1));
	
	err=dc1394_feature_set_value(x->camera, id, (uint32_t) jit_atom_getlong(argv+1));
	
	return JIT_ERR_NONE;
}

t_jit_err jit_iidc_setroi(t_jit_iidc *x, t_symbol *s, short argc, t_atom *argv)
{
	uint32_t left, top, width, height;
	dc1394error_t err;
	dc1394color_coding_t color_coding;
	uint32_t cpacket_size, cleft, ctop, cwidth, cheight;
	dc1394format7mode_t f7_mode;
	
	// exit if not exactly 4 arguments
	if ( argc != 4 )
	{
		printf("roi message needs 4 int arguments, got %d instead\n",argc);
		jit_object_error((t_object *)x, "setroi message needs 4 arguments");
		return JIT_ERR_GENERIC;
	}
	
	// exit if one arg is not A_LONG
	if (atom_gettype(argv)!=A_LONG ||
		atom_gettype(argv+1)!=A_LONG ||
		atom_gettype(argv+2)!=A_LONG ||
		atom_gettype(argv+3)!=A_LONG)
	{
		printf("roi message needs 4 int arguments\n");
		jit_object_error((t_object *)x, "roi message needs 4 int arguments");
		return JIT_ERR_GENERIC;
	}
	
	// exit if mode is not scalable
	if (!dc1394_is_video_mode_scalable(x->video_mode_id)){
		printf("roi is only available for scalable modes (format 6 & 7)\n");
		jit_object_error((t_object *)x, "roi is only available for scalable modes (format 6 & 7)");
		return JIT_ERR_NONE;
	}
	
	// get infos about the video mode
	err=dc1394_format7_get_mode_info(x->camera, x->video_mode_id, &f7_mode);
	JIT_DC1394_ERR_RTN(err,"Failed to get video mode infos\n");
	
	x->color_coding_id = f7_mode.color_coding;
	/*
	left	=	CLAMP(jit_atom_getlong(argv),	0,	f7_mode.max_size_x);
	top		=	CLAMP(jit_atom_getlong(argv+1),	0,	f7_mode.max_size_y);
	width	=	CLAMP(jit_atom_getlong(argv+2),	f7_mode.unit_size_x, f7_mode.max_size_x);
	height	=	CLAMP(jit_atom_getlong(argv+3),	f7_mode.unit_size_y, f7_mode.max_size_y);
	 */
	left = jit_atom_getlong(argv);
	top = jit_atom_getlong(argv+1);
	width = jit_atom_getlong(argv+2);
	height = jit_atom_getlong(argv+3);
	printf("left : %d, top : %d, width : %d, height : %d\n",left, top, width, height);
	printf("left & top must be multiple of %d & %d\n", f7_mode.unit_pos_x, f7_mode.unit_pos_y);
	printf("width & height must be multiple of %d & %d\n", f7_mode.unit_size_x, f7_mode.unit_size_y);
	printf("max size : %d, %d\n", f7_mode.max_size_x, f7_mode.max_size_y);
	
	// exit if left & top are not multiple of h_unit_pos & v_unit_pos
	if ( left % f7_mode.unit_pos_x || top % f7_mode.unit_pos_y ){
		printf("left & top must be multiple of %d & %d\n", f7_mode.unit_pos_x, f7_mode.unit_pos_y);
		jit_object_error((t_object *)x, "left & top must be multiple of %d & %d", f7_mode.unit_pos_x, f7_mode.unit_pos_y);
		return JIT_ERR_GENERIC;
	}
	
	// exit if width & height are not multiple of h_unit & v_unit
	if ( width % f7_mode.unit_size_x || height % f7_mode.unit_size_y){
		printf("width & height must be multiple of %d & %d\n", f7_mode.unit_size_x, f7_mode.unit_size_y);
		jit_object_error((t_object *)x, "width & height must be multiple of %d & %d\n", f7_mode.unit_size_x, f7_mode.unit_size_y);
		return JIT_ERR_GENERIC;
	}
	
	// exit if ROI isn't in the sensor area...
	if ( left+width>f7_mode.max_size_x  || top+height>f7_mode.max_size_y) {
		err = DC1394_FAILURE;
		JIT_DC1394_ERR_RTN(err,"ROI doesn't fit in the sensor area !\n");
	}
	
	printf("video mode : %s,color coding : %s, packet size : %d\n",x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name,x->color_coding_table[x->color_coding_id-DC1394_COLOR_CODING_MIN].s_name,f7_mode.packet_size);
	
	err=dc1394_format7_get_roi(x->camera, x->video_mode_id, &color_coding, &cpacket_size, &cleft, &ctop, &cwidth, &cheight);
	JIT_DC1394_ERR_RTN(err,"Can't get ROI form current video mode");
	
	printf("current RIO settings : \n");
	printf("colorcoding \t : %d\n",color_coding);
	printf("packet-size \t : %d \n",cpacket_size);
	printf("left \t : %d \n",cleft);
	printf("top \t : %d \n",ctop);
	printf("width \t : %d \n",cwidth);
	printf("height \t : %d \n",cheight);
	
	
	printf("set roi after some tests\n\n");
	
	err = dc1394_format7_set_roi(x->camera, x->video_mode_id, x->color_coding_id, f7_mode.packet_size, left, top, width, height);
	JIT_DC1394_ERR_RTN(err,"Can't set ROI");
	
	printf("SUCCESS !!!\n");
	
	err=dc1394_format7_get_roi(x->camera, x->video_mode_id, &color_coding, &cpacket_size, &cleft, &ctop, &cwidth, &cheight);
	if (err) {
		printf("can't get ROI from current video mode\n");
		jit_object_error((t_object *)x, "can't get ROI from current video mode");
		return JIT_ERR_GENERIC;
	}
	printf("new RIO settings : \n");
	printf("colorcoding \t : %d\n",color_coding);
	printf("packet-size \t : %d \n",cpacket_size);
	printf("left \t : %d \n",cleft);
	printf("top \t : %d \n",ctop);
	printf("width \t : %d \n",cwidth);
	printf("height \t : %d \n",cheight);
	
	
	printf("reset capture\n");
	// reset capture
	if(x->flags.transmission){
		dc1394_video_set_transmission(x->camera, DC1394_OFF);
		x->flags.transmission=0;
		if(x->flags.capture){
			dc1394_capture_stop(x->camera);
			x->flags.capture=0;
		}
		err=jit_iidc_setup_capture(x);
		JIT_DC1394_ERR_RTN(err,"Can't set the capture");
		
		dc1394_video_set_transmission(x->camera, DC1394_ON);
		x->flags.transmission=1;
	}
	printf("set ROI done\n");

	
	return JIT_ERR_NONE;
}

dc1394error_t jit_iidc_getbestsettings(t_jit_iidc *x)
{
	/*-----------------------------------------------------------------------
     *  get the best video mode and highest framerate. This can be skipped
     *  if you already know which mode/framerate you want...
     *-----------------------------------------------------------------------*/
	
	dc1394error_t err;
	int i;
	dc1394video_modes_t video_modes;			// list of supported videomodes
	
	if (x->camera == NULL) {
		jit_object_error((t_object *)x, "Camera not initialized");
		return DC1394_FAILURE;
	}
	
	// get video modes:
	err=dc1394_video_get_supported_modes(x->camera,&video_modes);
	JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Can't get video modes");
	
	// select highest res mode:
	printf("try to get a RGB8 mode\n");
	for (i=video_modes.num-1;i>=0;i--) {
		if (!dc1394_is_video_mode_scalable(video_modes.modes[i])) {
			dc1394_get_color_coding_from_video_mode(x->camera,video_modes.modes[i], &x->color_coding_id);
			if (x->color_coding_id==DC1394_COLOR_CODING_RGB8) 
			{
				x->video_mode_id=video_modes.modes[i];
				x->video_mode_sym.s_name = x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name;
				break;
			}
		}
	}
	
	if (i < 0) {
		printf("can't find a RGB8 mode, try to get a MONO8 mode\n");
		for (i=video_modes.num-1;i>=0;i--) {
			if (!dc1394_is_video_mode_scalable(video_modes.modes[i])) {
				dc1394_get_color_coding_from_video_mode(x->camera,video_modes.modes[i], &x->color_coding_id);
				if (x->color_coding_id==DC1394_COLOR_CODING_MONO8) 
				{
					x->video_mode_id=video_modes.modes[i];
					x->video_mode_sym.s_name = x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name;
					break;
				}
			}
		}
	}
	printf ("bestsettings (MONO8), i :%d\n",i);
	
	if (i<0) {
		for (i=video_modes.num-1;i>=0;i--) {
			if (!dc1394_is_video_mode_scalable(video_modes.modes[i])) {
				dc1394_get_color_coding_from_video_mode(x->camera,video_modes.modes[i], &x->color_coding_id);
				x->video_mode_id=video_modes.modes[i];
				x->video_mode_sym.s_name = x->video_mode_table[x->video_mode_id-DC1394_VIDEO_MODE_MIN].s_name;
				break;
				
			}
		}
	}
	
	if (i < 0) {
		jit_object_error((t_object *)x, "Could not get a valid color mode");
		jit_iidc_close(x);
	}
	
	// get highest framerate
	err=dc1394_video_get_supported_framerates(x->camera,x->video_mode_id,&x->framerates);
	JIT_DC1394_ERR_CLN_RTN(err,jit_iidc_close(x),"Could not get framerates");
	x->framerate_id = x->framerates.framerates[x->framerates.num-1];
	
	return DC1394_SUCCESS;

}

void jit_iidc_table_init(t_jit_iidc *x)
{
	// lookup table of camera's feature initialization 
	int i;
	int FORMAT7_OFFSET = DC1394_VIDEO_MODE_FORMAT7_MIN-DC1394_VIDEO_MODE_MIN;
	
	for (i=0; i<DC1394_FEATURE_NUM; i++) {
		switch (i) {
			case DC1394_FEATURE_BRIGHTNESS-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("brightness");
				break;
			case DC1394_FEATURE_EXPOSURE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("exposure");
				break;
			case DC1394_FEATURE_SHARPNESS-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("sharpness");
				break;
			case DC1394_FEATURE_WHITE_BALANCE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("white_balance");
				break;
			case DC1394_FEATURE_HUE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("hue");
				break;
			case DC1394_FEATURE_SATURATION-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("saturation");
				break;
			case DC1394_FEATURE_GAMMA-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("gamma");
				break;
			case DC1394_FEATURE_SHUTTER-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("shutter");
				break;
			case DC1394_FEATURE_GAIN-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("gain");
				break;
			case DC1394_FEATURE_IRIS-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("iris");
				break;
			case DC1394_FEATURE_FOCUS-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("focus");
				break;
			case DC1394_FEATURE_TEMPERATURE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("temperature");
				break;
			case DC1394_FEATURE_TRIGGER-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("trigger");
				break;
			case DC1394_FEATURE_TRIGGER_DELAY-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("trigger_delay");
				break;
			case DC1394_FEATURE_WHITE_SHADING-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("white_shading");
				break;
			case DC1394_FEATURE_FRAME_RATE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("framerate");
				break;
			case DC1394_FEATURE_ZOOM-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("zoom");
				break;
			case DC1394_FEATURE_PAN-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("pan");
				break;
			case DC1394_FEATURE_TILT-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("tilt");
				break;
			case DC1394_FEATURE_OPTICAL_FILTER-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("optical_filter");
				break;
			case DC1394_FEATURE_CAPTURE_SIZE-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("capture_size");
				break;
			case DC1394_FEATURE_CAPTURE_QUALITY-DC1394_FEATURE_MIN:
				x->feature_table[i]=*gensym("capture_quality");
				break;
			default:
				break;
		}
	}
	
	for (i=0; i<DC1394_VIDEO_MODE_NUM; i++) {
		switch (i) {
			case DC1394_VIDEO_MODE_160x120_YUV444-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("160x120_YUV444");
				break;
			case DC1394_VIDEO_MODE_320x240_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("320x240_YUV422");
				break;
			case DC1394_VIDEO_MODE_640x480_YUV411-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("640x480_YUV411");
				break;
			case DC1394_VIDEO_MODE_640x480_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("640x480_YUV422");
				break;
			case DC1394_VIDEO_MODE_640x480_RGB8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("640x480_RGB8");
				break;
			case DC1394_VIDEO_MODE_640x480_MONO8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("640x480_MONO8");
				break;
			case DC1394_VIDEO_MODE_640x480_MONO16-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("640x480_MONO16");
				break;
			case DC1394_VIDEO_MODE_800x600_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("800x600_YUV422");
				break;
			case DC1394_VIDEO_MODE_800x600_RGB8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("800x600_RGB8");
				break;
			case DC1394_VIDEO_MODE_800x600_MONO8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("800x600_MONO8");
				break;
			case DC1394_VIDEO_MODE_1024x768_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1024x768_YUV422");
				break;
			case DC1394_VIDEO_MODE_1024x768_RGB8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1024x768_RGB8");
				break;
			case DC1394_VIDEO_MODE_1024x768_MONO8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1024x768_MONO8");
				break;
			case DC1394_VIDEO_MODE_800x600_MONO16-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("800x600_MONO16");
				break;
			case DC1394_VIDEO_MODE_1024x768_MONO16-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1024x768_MONO16");
				break;
			case DC1394_VIDEO_MODE_1280x960_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1280x960_YUV422");
				break;
			case DC1394_VIDEO_MODE_1280x960_RGB8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1280x960_RGB8");
				break;
			case DC1394_VIDEO_MODE_1280x960_MONO8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1280x960_MONO8");
				break;
			case DC1394_VIDEO_MODE_1600x1200_YUV422-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1600x1200_YUV422");
				break;
			case DC1394_VIDEO_MODE_1600x1200_RGB8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1600x1200_RGB8");
				break;
			case DC1394_VIDEO_MODE_1600x1200_MONO8-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1600x1200_MONO8");
				break;
			case DC1394_VIDEO_MODE_1280x960_MONO16-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1280x960_MONO16");
				break;
			case DC1394_VIDEO_MODE_1600x1200_MONO16-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("1600x1200_MONO16");
				break;
			case DC1394_VIDEO_MODE_EXIF-DC1394_VIDEO_MODE_MIN:
				x->video_mode_table[i]=*gensym("MODE_EXIF");
				break;
			default:
				if(i>=DC1394_VIDEO_MODE_FORMAT7_MIN-DC1394_VIDEO_MODE_MIN || 
				   i<=DC1394_VIDEO_MODE_FORMAT7_MAX-DC1394_VIDEO_MODE_MIN)
				{
					char name[10];
					sprintf(name,"FORMAT7_MODE%d",i-FORMAT7_OFFSET);
					x->video_mode_table[i]=*gensym(name);
				}
				break;
		}
	}
	//post("name of video mode %d : %s",i,x->video_mode_table[i].s_name);
	
	for (i=0; i<DC1394_FEATURE_MODE_NUM;i++)
	{
		switch (i) {
			case DC1394_FEATURE_MODE_MANUAL-DC1394_FEATURE_MODE_MIN:
				x->feature_mode_table[i]=*gensym("manual");
				break;
			case DC1394_FEATURE_MODE_AUTO-DC1394_FEATURE_MODE_MIN:
				x->feature_mode_table[i]=*gensym("auto");
				break;
			case DC1394_FEATURE_MODE_ONE_PUSH_AUTO-DC1394_FEATURE_MODE_MIN:
				x->feature_mode_table[i]=*gensym("one_push_auto");
				break;
			default:
				break;
		}
	}
	
	for (i=0; i<DC1394_COLOR_CODING_NUM;i++)
	{
		switch (i) {
			case DC1394_COLOR_CODING_MONO8-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("MONO8");
				break;
			case DC1394_COLOR_CODING_YUV411-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("YUV411");
				break;
			case DC1394_COLOR_CODING_YUV422-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("YUV422");
				break;
			case DC1394_COLOR_CODING_YUV444-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("YUV444");
				break;
			case DC1394_COLOR_CODING_RGB8-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("RGB8");
				break;
			case DC1394_COLOR_CODING_MONO16-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("MONO16");
				break;
			case DC1394_COLOR_CODING_RGB16-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("RGB16");
				break;
			case DC1394_COLOR_CODING_MONO16S-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("MONO16S");
				break;
			case DC1394_COLOR_CODING_RGB16S-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("RGB16S");
				break;
			case DC1394_COLOR_CODING_RAW8-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("RAW8");
				break;
			case DC1394_COLOR_CODING_RAW16-DC1394_COLOR_CODING_MIN:
				x->color_coding_table[i]=*gensym("RAW16");
				break;
			default:
				break;
		}
	}
	for (i=0; i<DC1394_FRAMERATE_NUM;i++)
	{
		// I could do something like 1.875*2^i but I don't...
		switch (i) {
			case DC1394_FRAMERATE_1_875-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("1.875");
				break;
			case DC1394_FRAMERATE_3_75-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("3.75");
				break;
			case DC1394_FRAMERATE_7_5-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("7.5");
				break;
			case DC1394_FRAMERATE_15-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("15");
				break;
			case DC1394_FRAMERATE_30-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("30");
				break;
			case DC1394_FRAMERATE_60-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("60");
				break;
			case DC1394_FRAMERATE_120-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("120");
				break;
			case DC1394_FRAMERATE_240-DC1394_FRAMERATE_MIN:
				x->framerate_table[i]=*gensym("240");
				break;
		}
	}
}

dc1394feature_t jit_iidc_getfeatureid(t_jit_iidc *x, t_symbol *s)
{	
	dc1394feature_t id;
	for (id=DC1394_FEATURE_MIN; id<DC1394_FEATURE_MAX+1; id++) 
	{
		if (!strcmp(s->s_name, x->feature_table[id-DC1394_FEATURE_MIN].s_name)) {
			return id;
		}
	}
	return DC1394_FEATURE_MIN-1;
}

dc1394feature_t jit_iidc_getvideomodeid(t_jit_iidc *x, t_symbol *s)
{	
	dc1394feature_t id;
	for (id=DC1394_VIDEO_MODE_MIN; id<DC1394_VIDEO_MODE_MAX+1; id++) 
	{
		if (!strcmp(s->s_name, x->video_mode_table[id-DC1394_VIDEO_MODE_MIN].s_name)) {
			return id;
		}
	}
	return DC1394_VIDEO_MODE_MIN-1;
}

dc1394feature_t jit_iidc_getfeaturemodeid(t_jit_iidc *x, t_symbol *s)
{	
	dc1394feature_t id;
	for (id=DC1394_FEATURE_MODE_MIN; id<DC1394_FEATURE_MODE_MAX+1; id++) 
	{
		if (!strcmp(s->s_name, x->feature_mode_table[id-DC1394_FEATURE_MODE_MIN].s_name)) {
			return id;
		}
	}
	return DC1394_FEATURE_MODE_MIN-1;
}

dc1394color_coding_t jit_iidc_getcolorcodingid(t_jit_iidc *x, t_symbol *s)
{	
	dc1394color_coding_t id;
	for (id=DC1394_COLOR_CODING_MIN; id<DC1394_COLOR_CODING_MAX+1; id++) 
	{
		if (!strcmp(s->s_name, x->feature_mode_table[id-DC1394_COLOR_CODING_MIN].s_name)) {
			return id;
		}
	}
	return DC1394_COLOR_CODING_MIN-1;
}

dc1394framerate_t jit_iidc_getframerateid(t_jit_iidc *x, t_symbol *s)
{	
	dc1394framerate_t id;
	for (id=DC1394_FRAMERATE_MIN; id<DC1394_FRAMERATE_MAX+1; id++) 
	{
		printf("passed symbol : %s, framerate : %s, id : %d\n",s->s_name,x->framerate_table[id-DC1394_FRAMERATE_MIN].s_name,id);
		if (!strcmp(s->s_name, x->framerate_table[id-DC1394_FRAMERATE_MIN].s_name)) {
			printf("framerate id\n");
			return id;
		}
	}
	return DC1394_COLOR_CODING_MIN-1;
}