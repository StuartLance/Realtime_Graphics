//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
compute test.cs

\test.cs
#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() 
{
	vec4 i = vec4(0.0);
}

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_pos;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\quad.vs

#version 330 core

in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}


\texture.fs

#version 330 core

uniform float u_shininess;
uniform float u_specular_strength;
uniform vec3 u_ambient_color;
uniform vec3 u_camera_position;

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

uniform vec3 u_light_pos[10];
uniform vec3 u_light_color[10];
uniform float u_light_intensity[10];
uniform int u_light_count; // Put where the 4 is
uniform int u_light_type[10]; // 0 = point, 1 = directional

uniform vec3 u_multi_light_pos;
uniform vec3 u_multi_light_color;
uniform vec3 u_multi_light_dir;
uniform float u_multi_light_intensity;
uniform int u_multi_type;

uniform vec3 u_light_dir[10]; // direction for directional lights
uniform int u_multipass;     // 0 = single pass, 1 = multipass
uniform int u_light_index;   // Only used if u_multipass == 1

out vec4 FragColor;

uniform float u_alpha_max;
uniform float u_alpha_min;

uniform sampler2D u_normal_map;


mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv) {
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx(p);
  vec3 dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);

  // solve the linear system
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame 
  float invmax = 1.0 / sqrt(max(dot(T,T), dot(B,B)));
  return mat3(normalize(T * invmax), normalize(B * invmax), N);
}

vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel){
	normal_pixel = normal_pixel * 255./127. -128./127.;
	mat3 TBN = cotangentFrame(N, WP, uv);
	return normalize(TBN * normal_pixel);
}

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture(u_texture, v_uv);

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);
	vec3 total_light = vec3(0.0);

	// Tangent space normal mapping
	vec3 texture_normal = texture(u_normal_map, uv).xyz;
	texture_normal = (texture_normal * 2.0) - 1.0;
	texture_normal = normalize(texture_normal);
	vec3 normal = perturbNormal(v_normal, v_world_position, uv, texture_normal);
	N = normal; // Override world-space normal with perturbed normal

	int start_i = 0;
	int end_i = u_light_count;

	if (u_multipass == 1) {
		start_i = u_light_index;
		end_i = u_light_index + 1;
	}

	for (int i = 0; i < u_light_count; ++i) {
		if (i < start_i || i >= end_i)
			continue;

		vec3 L;
		float attenuation = 1.0;

		vec3 L_unnorm = u_light_pos[i] - v_world_position;
		float d = length(L_unnorm);

		if (u_light_type[i] == 1) {
			// Point light
			L = normalize(L_unnorm);
			d = max(d, 0.01);
			attenuation = 1.0 / (d * d);
		} else if (u_light_type[i] == 3) {
			// Directional light
			L = normalize(-normalize(u_light_dir[i]));
			attenuation = 1.0;
		} else if (u_light_type[i] == 2) {
			// Spotlight
			vec3 D = normalize(u_light_dir[i]);
			L = normalize(L_unnorm);
			d = max(d, 0.01);
			attenuation = 1.0 / (d * d);

			float cos_theta = dot(L, D);
			float cutoff_outer = cos(u_alpha_max);
			float cutoff_inner = cos(u_alpha_min);

			if (cos_theta < cutoff_outer) {
				attenuation = 0.0;
			} else {
				float falloff = clamp((cos_theta - cutoff_outer) / (cutoff_inner - cutoff_outer), 0.0, 1.0);
				attenuation *= falloff;
			}
		}

		// Lighting equation
		vec3 R = reflect(-L, N);
		float diff = max(dot(N, L), 0.0);
		float spec = pow(max(dot(R, V), 0.0), u_shininess);

		vec3 light_color = u_light_color[i] * u_light_intensity[i];
		total_light += attenuation * (diff + spec * u_specular_strength) * light_color;
	}

	vec3 ambient = u_ambient_color * color.rgb;
	vec3 final_color = ambient + total_light * color.rgb;

	FragColor = vec4(final_color, color.a);
}




\skybox.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E );
	FragColor = color;
}


\multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, uv );

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N,1.0);
}


\depth.fs

#version 330 core

uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture; //depth map
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture2D(u_texture,v_uv).x;
	if( n == 0.0 && f == 1.0 )
		FragColor = vec4(z);
	else
		FragColor = vec4( n * (z + 1.0) / (f + n - z * (f - n)) );
}


\instanced.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

in mat4 u_model;

uniform vec3 u_camera_pos;

uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( a_vertex, 1.0) ).xyz;
	
	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}