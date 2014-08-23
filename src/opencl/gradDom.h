const char *gradDom_kernel =
"// gradDom.cl (HDR)\n"
"// Copyright (c) 2014, Amir Chohan,\n"
"// University of Bristol. All rights reserved.\n"
"//\n"
"// This program is provided under a three-clause BSD license. For full\n"
"// license terms please see the LICENSE file distributed with this\n"
"// source code.\n"
"\n"
"float GL_to_CL(uint val);\n"
"float3 RGBtoXYZ(float3 rgb);\n"
"\n"
"const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;\n"
"\n"
"//this kernel computes logLum\n"
"kernel void computeLogLum( 	__read_only image2d_t image,\n"
"							__global float* logLum) {\n"
"\n"
"	int2 pos;\n"
"	uint4 pixel;\n"
"	float lum;\n"
"	for (pos.y = get_global_id(1); pos.y < HEIGHT; pos.y += get_global_size(1)) {\n"
"		for (pos.x = get_global_id(0); pos.x < WIDTH; pos.x += get_global_size(0)) {\n"
"			pixel = read_imageui(image, sampler, pos);\n"
"			lum = GL_to_CL(pixel.x)*0.2126\n"
"				+ GL_to_CL(pixel.y)*0.7152\n"
"				+ GL_to_CL(pixel.z)*0.0722;\n"
"			logLum[pos.x + pos.y*WIDTH] = log(lum + 0.000001);\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"//computes the next level mipmap\n"
"kernel void channel_mipmap(	__global float* mipmap,	//array containing all the mipmap levels\n"
"							const int prev_width,	//width of the previous mipmap\n"
"							const int prev_offset, 	//start point of the previous mipmap \n"
"							const int m_width,		//width of the mipmap being generated\n"
"							const int m_height,		//height of the mipmap being generated\n"
"							const int m_offset) { 	//start point to store the current mipmap\n"
"	int2 pos;\n"
"	for (pos.y = get_global_id(1); pos.y < m_height; pos.y += get_global_size(1)) {\n"
"		for (pos.x = get_global_id(0); pos.x < m_width; pos.x += get_global_size(0)) {\n"
"			int _x = 2*pos.x;\n"
"			int _y = 2*pos.y;\n"
"			mipmap[pos.x + pos.y*m_width + m_offset] = 	(mipmap[_x + _y*prev_width + prev_offset]\n"
"														+ mipmap[_x+1 + _y*prev_width + prev_offset]\n"
"														+ mipmap[_x + (_y+1)*prev_width + prev_offset]\n"
"														+ mipmap[(_x+1) + (_y+1)*prev_width + prev_offset])/4.f;\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"//computing gradient magnitude using central differences at level k\n"
"kernel void gradient_mag(	__global float* lum,		//array containing all the luminance mipmap levels\n"
"							__global float* gradient,	//array to store all the gradients at different levels\n"
"							const int g_width,			//width of the gradient being generated\n"
"							const int g_height,			//height of the gradient being generated\n"
"							const int offset,			//start point to store the current gradient\n"
"							const float divider) { 	\n"
"	//k_av_grad = 0.f;\n"
"	int x_west;\n"
"	int x_east;\n"
"	int y_north;\n"
"	int y_south;\n"
"	float x_grad;\n"
"	float y_grad;\n"
"	int2 pos;\n"
"	for (pos.y = get_global_id(1); pos.y < g_height; pos.y += get_global_size(1)) {\n"
"		for (pos.x = get_global_id(0); pos.x < g_width; pos.x += get_global_size(0)) {\n"
"			x_west\t= clamp(pos.x-1, 0, g_width-1);\n"
"			x_east\t= clamp(pos.x+1, 0, g_width-1);\n"
"			y_north = clamp(pos.y-1, 0, g_height-1);\n"
"			y_south = clamp(pos.y+1, 0, g_height-1);\n"
"\n"
"			x_grad = (lum[x_west + pos.y*g_width + offset]\t- lum[x_east + pos.y*g_width + offset])/divider;\n"
"			y_grad = (lum[pos.x + y_south*g_width + offset] - lum[pos.x + y_north*g_width + offset])/divider;\n"
"\n"
"			gradient[pos.x + pos.y*g_width + offset] = sqrt(pow(x_grad, 2.f) + pow(y_grad, 2.f));\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"//used to compute the average gradient for the specified mipmap level\n"
"kernel void partialReduc(	__global float* gradient,	//array containing all the luminance gradient mipmap levels\n"
"							__global float* gradient_partial_sum,\n"
"							__local float* gradient_loc,\n"
"							const int height,	//height of the given mipmap\n"
"							const int width,	//width of the given mipmap\n"
"							const int g_offset) {	///index denoting the start point of the mipmap gradient array\n"
"\n"
"	float gradient_acc = 0.f;\n"
"\n"
"	for (int gid = get_global_id(0); gid < height*width; gid += get_global_size(0)) {\n"
"		gradient_acc += gradient[g_offset + gid];\n"
"	}\n"
"\n"
"	const int lid = get_local_id(0);	//local id in one dimension\n"
"	gradient_loc[lid] = gradient_acc;\n"
"\n"
"	// Perform parallel reduction\n"
"	barrier(CLK_LOCAL_MEM_FENCE);\n"
"\n"
"	for(int offset = get_local_size(0)/2; offset > 0; offset = offset/2) {\n"
"		if (lid < offset) {\n"
"			gradient_loc[lid] += gradient_loc[lid + offset];\n"
"		}\n"
"		barrier(CLK_LOCAL_MEM_FENCE);\n"
"	}\n"
"\n"
"	const int group_id = get_group_id(0);\n"
"	if (lid == 0) {\n"
"		gradient_partial_sum[group_id] = gradient_loc[0];\n"
"	}\n"
"}\n"
"\n"
"//used to compute the average gradient for the specified mipmap level\n"
"kernel void finalReduc(	__global float* gradient_partial_sum,\n"
"						__global float* alphas,	//array containg alpha for each mipmap level\n"
"						const int mipmap_level,\n"
"						const int width,	//width of the given mipmap\n"
"						const int height,	//height of the given mipmap\n"
"						const unsigned int num_reduc_bins) {\n"
"	if (get_global_id(0)==0) {\n"
"\n"
"		float sum_grads = 0.f;\n"
"	\n"
"		for (int i=0; i<num_reduc_bins; i++) {\n"
"			sum_grads += gradient_partial_sum[i];\n"
"		}\n"
"		alphas[mipmap_level] = ADJUST_ALPHA*exp(sum_grads/((float)width*height));\n"
"	}\n"
"	else return;\n"
"}\n"
"\n"
"//computes attenuation function of the coarsest level mipmap\n"
"kernel void coarsest_level_attenfunc(	__global float* gradient,	//array containing all the luminance gradient mipmap levels\n"
"										__global float* atten_func,	//arrray to store attenuation function for each mipmap\n"
"										__global float* k_alpha,	//array containing alpha for each mipmap\n"
"										const int width,	//width of the coarsest level mipmap\n"
"										const int height,	//height of the coarsest level mipmap\n"
"										const int offset) {	//index where the data about the coarsest level mipmap starts in gradient array and atten_func array\n"
"\n"
"	for (int gid = get_global_id(0); gid < width*height; gid+= get_global_size(0) ) {\n"
"		atten_func[gid+offset] = (k_alpha[0]/gradient[gid+offset])*pow(gradient[gid+offset]/k_alpha[0], (float)BETA);\n"
"	}\n"
"}\n"
"\n"
"//computes attenuation function of a given mipmap\n"
"kernel void atten_func(	__global float* gradient,	//array containing all the luminance gradient mipmap levels\n"
"						__global float* atten_func,	//arrray to store attenuation function for each mipmap\n"
"						__global float* k_alpha,	//array containing alpha for each mipmap\n"
"						const int width,	//width of the given mipmap\n"
"						const int height,	//height of the given mipmap\n"
"						const int offset,	//index where the data about the given mipmap level starts in gradient array and atten_func array\n"
"						const int c_width,	//width of the coarser mipmap\n"
"						const int c_height,	//height of the coarser mipmap\n"
"						const int c_offset,	//index where the data about the coarser mipmap level starts in gradient array and atten_func array\n"
"						const int level) {	//current mipmap level\n"
"	int2 pos;\n"
"	int2 c_pos;\n"
"	int2 neighbour;\n"
"	float k_xy_atten_func;\n"
"	float k_xy_scale_factor;\n"
"	for (pos.y = get_global_id(1); pos.y < height; pos.y += get_global_size(1)) {\n"
"		for (pos.x = get_global_id(0); pos.x < width; pos.x += get_global_size(0)) {\n"
"			if (gradient[pos.x + pos.y*width + offset] != 0) {\n"
"\n"
"				c_pos = pos/2;	//position in the coarser grid\n"
"\n"
"				//neighbours need to be left or right dependent on where we are\n"
"				neighbour.x = (pos.x & 1) ? 1 : -1;\n"
"				neighbour.y = (pos.y & 1) ? 1 : -1;\n"
"\n"
"\n"
"				//this stops us from going out of bounds\n"
"				if ((c_pos.x + neighbour.x) < 0) neighbour.x = 0;\n"
"				if ((c_pos.y + neighbour.y) < 0) neighbour.y = 0;\n"
"				if ((c_pos.x + neighbour.x) >= c_width)\tneighbour.x = 0;\n"
"				if ((c_pos.y + neighbour.y) >= c_height) neighbour.y = 0;\n"
"				if (c_pos.x == c_width)\tc_pos.x -= 1;\n"
"				if (c_pos.y == c_height) c_pos.y -= 1;\n"
"\n"
"				k_xy_atten_func = 9.0*atten_func[c_pos.x 				+ c_pos.y					*c_width	+ c_offset]\n"
"								+ 3.0*atten_func[c_pos.x+neighbour.x 	+ c_pos.y					*c_width	+ c_offset]\n"
"								+ 3.0*atten_func[c_pos.x 				+ (c_pos.y+neighbour.y)		*c_width	+ c_offset]\n"
"								+ 1.0*atten_func[c_pos.x+neighbour.x 	+ (c_pos.y+neighbour.y)		*c_width	+ c_offset];\n"
"\n"
"				k_xy_scale_factor = (k_alpha[level]/gradient[pos.x + pos.y*width + offset])*pow(gradient[pos.x + pos.y*width + offset]/k_alpha[level], (float)BETA);\n"
"				atten_func[pos.x + pos.y*width + offset] = (1.f/16.f)*(k_xy_atten_func)*k_xy_scale_factor;\n"
"			}\n"
"			else atten_func[pos.x + pos.y*width + offset] = 0.f;\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"//finds gradients in x and y direction and attenuates them using the previously computed attenuation function\n"
"kernel void grad_atten(	__global float* atten_grad_x,	//array to store the attenuated gradient in x dimension\n"
"						__global float* atten_grad_y,	//array to store the attenuated gradeint in y dimension\n"
"						__global float* lum,			//original luminance of the image\n"
"						__global float* atten_func) {	//attenuation function\n"
"	int2 pos;\n"
"	float2 grad;\n"
"	for (pos.y = get_global_id(1); pos.y < HEIGHT; pos.y += get_global_size(1)) {\n"
"		for (pos.x = get_global_id(0); pos.x < WIDTH; pos.x += get_global_size(0)) {	\n"
"			grad.x = (pos.x < WIDTH-1 ) ? (lum[pos.x+1 +\t	 pos.y*WIDTH] - lum[pos.x + pos.y*WIDTH]) : 0;\n"
"			grad.y = (pos.y < HEIGHT-1) ? (lum[pos.x\t + (pos.y+1)*WIDTH] - lum[pos.x + pos.y*WIDTH]) : 0;\n"
"			atten_grad_x[pos.x + pos.y*WIDTH] = grad.x*atten_func[pos.x + pos.y*WIDTH];\n"
"			atten_grad_y[pos.x + pos.y*WIDTH] = grad.y*atten_func[pos.x + pos.y*WIDTH];\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"//computes the divergence field of the attenuated gradients\n"
"kernel void divG(	__global float* atten_grad_x,	//attenuated gradient in x direction\n"
"					__global float* atten_grad_y,	//attenuated gradient in y direction\n"
"					__global float* div_grad) {		//array to store the divergence field of the gradients\n"
"	div_grad[0] = 0;\n"
"	int2 pos;\n"
"	for (pos.x = get_global_id(0) + 1; pos.x < WIDTH; pos.x += get_global_size(0)) {\n"
"		div_grad[pos.x] = atten_grad_x[pos.x] - atten_grad_x[pos.x-1];\n"
"	}\n"
"	for (pos.y = get_global_id(1) + 1; pos.y < HEIGHT; pos.y += get_global_size(1)) {\n"
"		div_grad[pos.y*WIDTH] = atten_grad_y[pos.y*WIDTH] - atten_grad_y[(pos.y-1)*WIDTH];\n"
"		for (pos.x = get_global_id(0)+1; pos.x < WIDTH; pos.x += get_global_size(0)) {\n"
"			div_grad[pos.x + pos.y*WIDTH] 	= (atten_grad_x[pos.x + pos.y*WIDTH] - atten_grad_x[(pos.x-1) + pos.y*WIDTH])\n"
"											+ (atten_grad_y[pos.x + pos.y*WIDTH] - atten_grad_y[pos.x + (pos.y-1)*WIDTH]);\n"
"		}\n"
"	}	\n"
"}\n"
"\n"
"\n"
"//convert an RGB pixel to XYZ format\n"
"float3 RGBtoXYZ(float3 rgb) {\n"
"	float3 xyz;\n"
"	xyz.x = rgb.x*0.4124 + rgb.y*0.3576 + rgb.z*0.1805;\n"
"	xyz.y = rgb.x*0.2126 + rgb.y*0.7152 + rgb.z*0.0722;\n"
"	xyz.z = rgb.x*0.0193 + rgb.y*0.1192 + rgb.z*0.9505;\n"
"	return xyz;\n"
"}\n"
"\n"
"//a function to read an OpenGL texture pixel when using Snapdragon's Android OpenCL implementation\n"
"float GL_to_CL(uint val) {\n"
"	if (BUGGY_CL_GL) {\n"
"		if (val >= 14340) return round(0.1245790*val - 1658.44);	//>=128\n"
"		if (val >= 13316) return round(0.0622869*val - 765.408);	//>=64\n"
"		if (val >= 12292) return round(0.0311424*val - 350.800);	//>=32\n"
"		if (val >= 11268) return round(0.0155702*val - 159.443);	//>=16\n"
"	\n"
"		float v = (float) val;\n"
"		return round(0.0000000000000125922*pow(v,4.f) - 0.00000000026729*pow(v,3.f) + 0.00000198135*pow(v,2.f) - 0.00496681*v - 0.0000808829);\n"
"	}\n"
"	else return (float)val;\n"
"}\n";
