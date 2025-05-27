//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
gBuffer quad.vs deferred.fs
singlepass_deferred quad.vs singlepass_deferred.fs
fill basic.vs fill.fs
volume basic.vs volume.fs
ssao basic.vs ssao.fs

fire fire.vs fire.fs

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_position;

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

\ssao.fs
#version 330 core

in vec2 v_uv;
out vec4 FragColor;

// G-Buffer
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_normal;

// SSAO
uniform vec3 u_sample_pos[64]; // Higher than 32
uniform int u_sample_count;
uniform float u_sample_radius;
uniform sampler2D u_noise_texture;
uniform vec2 u_noise_scale;

// Matrices
uniform mat4 u_p_mat;
uniform mat4 u_inv_p_mat;

vec3 reconstructViewPos(vec2 uv, float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view = u_inv_p_mat * clip;
    return view.xyz / view.w;
}

void main()
{
    float center_depth = texture(u_gbuffer_depth, v_uv).r;
    if (center_depth >= 1.0)
        discard;

    vec3 origin = reconstructViewPos(v_uv, center_depth);

    // Reconstruct and normalize normal from G-Buffer
    vec3 normal = texture(u_gbuffer_normal, v_uv).xyz * 2.0 - 1.0;
    normal = normalize(normal);

    // Sample random rotation vector
    vec2 noise_uv = v_uv * u_noise_scale;
    vec3 random_vec = texture(u_noise_texture, noise_uv).xyz;

    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < u_sample_count; ++i)
    {
        vec3 sample_vec = TBN * u_sample_pos[i];  // orientació SSAO+
        vec3 sample_pos = origin + sample_vec * u_sample_radius;

        vec4 proj = u_p_mat * vec4(sample_pos, 1.0);
        proj.xyz /= proj.w;
        vec2 sample_uv = proj.xy * 0.5 + 0.5;

        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0)
            continue;

        float sample_depth = texture(u_gbuffer_depth, sample_uv).r;
        vec3 sample_view = reconstructViewPos(sample_uv, sample_depth);

        // Si el punt mostrat està més a prop que la mostra → occlusion
        if (sample_view.z < sample_pos.z - 0.01)
            occlusion += 1.0;
    }

    occlusion = 1.0 - (occlusion / float(u_sample_count));
    FragColor = vec4(vec3(occlusion), 1.0);
}



\singlepass_deferred.fs
#version 410 core

#define MAX_LIGHTS 10

in vec2 uv;

// G-Buffer textures
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

// SSAO
uniform sampler2D u_ssao_texture;
uniform bool u_ssao_to_lighting;
uniform bool u_ssao_enabled;

// Camera info
uniform mat4 u_inverse_viewprojection;
uniform vec3 u_camera_position;

// Lighting
uniform vec3 u_ambient_light;
uniform int u_light_count;

uniform vec3 u_light_pos[MAX_LIGHTS];
uniform vec3 u_light_color[MAX_LIGHTS];
uniform float u_light_intensity[MAX_LIGHTS];
uniform int u_light_type[MAX_LIGHTS];        // 1 = point, 2 = spot, 3 = directional
uniform vec3 u_light_dir[MAX_LIGHTS];
uniform vec2 u_light_cone[MAX_LIGHTS];       // [inner, outer] angles in degrees

uniform vec2 u_res_inv;

out vec4 FragColor;

// === Utility Functions ===
vec3 reconstructPosition(vec2 uv, float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view = u_inverse_viewprojection * clip;
    return view.xyz / view.w;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (3.14159265 * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metalness) {
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);
    vec3 diffuse = albedo / 3.14159265;

    return (kD * diffuse + specular) * NdotL;
}

// === Main ===
void main()
{
    vec2 uv = gl_FragCoord.xy * u_res_inv;

    // Reconstruct data from GBuffer
    vec4 albedoRough = texture(u_gbuffer_color, uv);
    vec3 albedo = albedoRough.rgb;
    float roughness = albedoRough.a;

    vec4 normalMetal = texture(u_gbuffer_normal, uv);
    vec3 N = normalize(normalMetal.rgb * 2.0 - 1.0);
    float metalness = normalMetal.a;

    float depth = texture(u_gbuffer_depth, uv).r;
    if (depth >= 1.0)
        discard;

    vec3 world_pos = reconstructPosition(uv, depth);
    vec3 V = normalize(u_camera_position - world_pos);

    float ao = 1.0;
    if (u_ssao_enabled && u_ssao_to_lighting)
        ao = texture(u_ssao_texture, uv).r;

    vec3 final_color = albedo * u_ambient_light * ao;

    for (int i = 0; i < u_light_count && i < MAX_LIGHTS; ++i)
    {
        vec3 L;
        float attenuation = 1.0;
        float spotlight = 1.0;

        if (u_light_type[i] == 1) { // Point light
            vec3 light_vec = u_light_pos[i] - world_pos;
            float dist = length(light_vec);
            L = normalize(light_vec);
            attenuation = 1.0 / (dist * dist);
        }
        else if (u_light_type[i] == 2) { // Spot light
            vec3 light_vec = u_light_pos[i] - world_pos;
            float dist = length(light_vec);
            L = normalize(-light_vec);

            vec3 light_dir = normalize(u_light_dir[i]);
            float theta = dot(-L, light_dir);

            float outer = cos(radians(u_light_cone[i].y));
            float inner = cos(radians(u_light_cone[i].x));
            float epsilon = max(inner - outer, 0.001);
            spotlight = clamp((theta - outer) / epsilon, 0.0, 1.0);

            attenuation = spotlight / (dist * dist);
        }
        else if (u_light_type[i] == 3) { // Directional
            L = normalize(-u_light_dir[i]);
            attenuation = 1.0;
        }

        vec3 brdf = cookTorranceBRDF(N, V, L, albedo, roughness, metalness);
        vec3 light_color = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight;
        final_color += brdf * light_color;
    }

    FragColor = vec4(final_color, 1.0);
}



\deferred.fs

#version 410 core

in vec2 v_uv; // UV coordinates from the vertex shader

// G-buffer textures
uniform sampler2D u_gbuffer_color;  // Albedo texture
uniform sampler2D u_gbuffer_normal; // Normal texture
uniform sampler2D u_gbuffer_depth;  // Depth texture

// Light and camera uniforms
uniform mat4 u_inv_projection;      // Inverse projection matrix
uniform vec3 u_camera_position;     // Camera position
uniform vec3 u_light_pos;           // Light position
uniform vec3 u_light_color;         // Light color
uniform float u_light_intensity;    // Light intensity

out vec4 FragColor; // Final output color

// Function to reconstruct world-space position from depth
vec3 reconstructPosition(vec2 uv, float depth, mat4 invProjection) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0); // NDC
    vec4 viewSpacePosition = invProjection * clipSpacePosition; // View space
    viewSpacePosition /= viewSpacePosition.w; // Perspective divide
    return viewSpacePosition.xyz; // World-space position
}

void main() {
    // Sample G-buffer textures
    vec4 albedo = texture(u_gbuffer_color, v_uv);  // Albedo (color)
    vec4 normal_mat = texture(u_gbuffer_normal, v_uv); // Normal
    float depth = texture(u_gbuffer_depth, v_uv).r; // Depth

    // Reconstruct world-space position
    vec3 position = reconstructPosition(v_uv, depth, u_inv_projection);

    // Extract and normalize the normal
    vec3 normal = normalize(normal_mat.xyz);

    // Phong shading calculations
    vec3 lightDir = normalize(u_light_pos - position); // Light direction
    vec3 viewDir = normalize(u_camera_position - position); // View direction
    vec3 reflectDir = reflect(-lightDir, normal); // Reflected light direction

    // Ambient component
    vec3 ambient = 0.1 * albedo.rgb;

    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * u_light_color * u_light_intensity;

    // Specular component
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0); // Shininess = 32
    vec3 specular = spec * u_light_color * u_light_intensity;

    // Combine all components
    vec3 finalColor = ambient + diffuse + specular;

    // Output the final color
    FragColor = vec4(finalColor, albedo.a);
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}

\fill.fs
#version 330 core
// Uniforms
uniform sampler2D u_texture; // Albedo
uniform sampler2D u_normal_texture;
uniform sampler2D u_metallic_roughness;

in vec2 v_uv;

layout(location = 0) out vec4 gbuffer1;
layout(location = 1) out vec4 gbuffer2;

void main() {
    vec3 albedo = texture(u_texture, v_uv).rgb;
    vec3 normal = texture(u_normal_texture, v_uv).rgb;
    vec3 mer = texture(u_metallic_roughness, v_uv).rgb;

    float roughness = mer.g;
    float metalness = mer.b;

    gbuffer1 = vec4(albedo, roughness);
    gbuffer2 = vec4(normal, metalness);
}

\texture.fs

#version 410 core

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
uniform int u_lab;

//Physically Based Renderer
//uniform sampler2D u_texture; // ALBEDO
uniform sampler2D u_normal_texture; // NORMALMAP
uniform sampler2D u_metallic_roughness_texture; // R: AO, G: Roughness, B:Metalness

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
    normal_pixel = normal_pixel;
    mat3 TBN = cotangentFrame(N, WP, uv);
    return normalize(TBN * normal_pixel);
}
layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normal_mat;

void single_multi()
{
    vec2 uv = v_uv;
    vec4 color = u_color;
    color *= texture(u_texture, v_uv);

    if (color.a < u_alpha_cutoff)
        discard;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camera_position - v_world_position);

    // Tangent space normal mapping
    vec3 texture_normal = texture(u_normal_map, uv).xyz;
    texture_normal = (texture_normal * 2.0) - 1.0;
    vec3 normal = perturbNormal(v_normal, v_world_position, uv, texture_normal);
    N = normal;

    vec3 total_diff = vec3(0.0);
    vec3 total_spec = vec3(0.0);
    vec3 ambient = vec3(0.0);

    if (u_multipass == 1) {
        // Multi-pass: process only one light, provided by u_light_index
        int i = u_light_index;

        if (i == 0) {
            ambient = u_ambient_color * color.rgb;
        }

        vec3 L_unnorm = u_light_pos[i] - v_world_position;
        vec3 L;
        float attenuation = 1.0;
        float d = length(L_unnorm);

        // Light direction and attenuation
        if (u_light_type[i] == 1) {
            L = normalize(L_unnorm);
            d = max(d, 0.01);
            attenuation = 1.0 / (d * d);
        } else if (u_light_type[i] == 3) {
            L = normalize(-normalize(u_light_dir[i]));
            attenuation = 1.0;
        } else if (u_light_type[i] == 2) {
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

        total_diff += attenuation * diff * light_color;
        total_spec += attenuation * spec * u_specular_strength * light_color;
    }
    else {
        // Single-pass: loop through all lights
        ambient = u_ambient_color * color.rgb;

        for (int i = 0; i < u_light_count; ++i) {
            vec3 L_unnorm = u_light_pos[i] - v_world_position;
            vec3 L;
            float attenuation = 1.0;
            float d = length(L_unnorm);

            if (u_light_type[i] == 1) {
                L = normalize(L_unnorm);
                d = max(d, 0.01);
                attenuation = 1.0 / (d * d);
            } else if (u_light_type[i] == 3) {
                L = normalize(-normalize(u_light_dir[i]));
                attenuation = 1.0;
            } else if (u_light_type[i] == 2) {
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

            // Lighting equation inside the loop
            vec3 R = reflect(-L, N);
            float diff = max(dot(N, L), 0.0);
            float spec = pow(max(dot(R, V), 0.0), u_shininess);
            vec3 light_color = u_light_color[i] * u_light_intensity[i];

            total_diff += attenuation * diff * light_color;
            total_spec += attenuation * spec * u_specular_strength * light_color;
        }
    }

    vec3 final_color = ambient + total_diff * color.rgb + total_spec;
    FragColor = vec4(N, color.a);
}

layout(location = 0) in vec2 a_position; // Quad vertex positions in NDC

void gBuffer()
{
    vec2 uv = v_uv;
    vec4 color = u_color * texture(u_texture, uv);

    if (color.a < u_alpha_cutoff)
        discard;

    // Normal mapping
    vec3 texture_normal = texture(u_normal_map, uv).xyz;
    texture_normal = (texture_normal * 2.0) - 1.0;
    vec3 normal = perturbNormal(v_normal, v_world_position, uv, texture_normal);

    // Store to G-buffer
    gbuffer_albedo = color;              // Store base color (albedo)
    gbuffer_normal_mat = vec4(normal, 1.0); // Store world-space normal
}

void physical() {
    vec2 uv = v_uv;
    vec4 tex_color = texture(u_texture, uv);
    if (tex_color.a < u_alpha_cutoff)
        discard;

    vec3 albedo = tex_color.rgb * u_color.rgb;
    float alpha = tex_color.a * u_color.a;

    // Normal mapping
    vec3 texture_normal = texture(u_normal_texture, uv).rgb;
    texture_normal = normalize(texture_normal * 2.0 - 1.0);
    vec3 N = perturbNormal(normalize(v_normal), v_world_position, uv, texture_normal);

    vec3 V = normalize(u_camera_position - v_world_position); // View vector

    // Load PBR properties
    vec3 mr_data = texture(u_metallic_roughness_texture, uv).rgb;
    float ao = mr_data.r;
    float roughness = clamp(mr_data.g, 0.05, 1.0);
    float metalness = mr_data.b;

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    vec3 Lo = vec3(0.0);

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
        vec3 light_color = u_light_color[i] * u_light_intensity[i];

        vec3 L_unnorm = u_light_pos[i] - v_world_position;
        float d = length(L_unnorm);

        if (u_light_type[i] == 1) {
            L = normalize(L_unnorm);
            d = max(d, 0.01);
            attenuation = 1.0 / (d * d);
        } else if (u_light_type[i] == 3) {
            L = normalize(-u_light_dir[i]);
            attenuation = 1.0;
        } else if (u_light_type[i] == 2) {
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

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        float rSqu = roughness*roughness;
        float denom_step = NdotH * NdotH * (rSqu * rSqu -1.0 ) + 1.0;
        float denom = 3.1415926 * pow(denom_step, 2);



        // Cook-Torrance BRDF
        float D = pow(rSqu, 2) / denom;
        float k = rSqu / 2.0;
        float G = (NdotL / (NdotL * (1.0 - k) + k)) * (NdotV / (NdotV * (1.0 - k) + k));
        vec3 F = F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);

        vec3 specular = D * G * F / max(4.0 * NdotL * NdotV, 0.001);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness;

        vec3 radiance = light_color * attenuation;

        vec3 contribution = ((albedo / 3.14159) + specular) * radiance * NdotL;
        Lo += contribution;
    }

    vec3 ambient = u_ambient_color * albedo * ao;
    vec3 final_color = ambient + Lo;
    FragColor = vec4(final_color, alpha);
}

void main()
{
    physical();
    //gBuffer();
}


\physical.fs


\singlepass.fs
#version 410 core

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
    normal_pixel = normal_pixel;
    mat3 TBN = cotangentFrame(N, WP, uv);
    return normalize(TBN * normal_pixel);
}

void main()
{
    vec2 uv = v_uv;
    vec4 color = u_color;
    color *= texture(u_texture, v_uv);

    if (color.a < u_alpha_cutoff)
        discard;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camera_position - v_world_position);

    // Tangent space normal mapping
    vec3 texture_normal = texture(u_normal_map, uv).xyz;
    texture_normal = (texture_normal * 2.0) - 1.0;
    vec3 normal = perturbNormal(v_normal, v_world_position, uv, texture_normal);
    N = normal;

    vec3 total_diff = vec3(0.0);
    vec3 total_spec = vec3(0.0);
    vec3 ambient = vec3(0.0);


    // Single-pass: loop through all lights
    ambient = u_ambient_color * color.rgb;

    for (int i = 0; i < u_light_count; ++i) {
        vec3 L_unnorm = u_light_pos[i] - v_world_position;
        vec3 L;
        float attenuation = 1.0;
        float d = length(L_unnorm);

        if (u_light_type[i] == 1) {
            L = normalize(L_unnorm);
            d = max(d, 0.01);
            attenuation = 1.0 / (d * d);
        } else if (u_light_type[i] == 3) {
            L = normalize(-normalize(u_light_dir[i]));
            attenuation = 1.0;
        } else if (u_light_type[i] == 2) {
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

        // Lighting equation inside the loop
        vec3 R = reflect(-L, N);
        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(R, V), 0.0), u_shininess);
        vec3 light_color = u_light_color[i] * u_light_intensity[i];

        total_diff += attenuation * diff * light_color;
        total_spec += attenuation * spec * u_specular_strength * light_color;
    }


    vec3 final_color = ambient + total_diff * color.rgb + total_spec;
    FragColor = vec4(N, color.a);
}




\multipass.fs

#version 410 core

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
    normal_pixel = normal_pixel;
    mat3 TBN = cotangentFrame(N, WP, uv);
    return normalize(TBN * normal_pixel);
}


void main()
{
    vec2 uv = v_uv;
    vec4 color = u_color;
    color *= texture(u_texture, v_uv);

    if (color.a < u_alpha_cutoff)
        discard;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camera_position - v_world_position);

    // Tangent space normal mapping
    vec3 texture_normal = texture(u_normal_map, uv).xyz;
    texture_normal = (texture_normal * 2.0) - 1.0;
    vec3 normal = perturbNormal(v_normal, v_world_position, uv, texture_normal);
    N = normal;

    vec3 total_diff = vec3(0.0);
    vec3 total_spec = vec3(0.0);
    vec3 ambient = vec3(0.0);


    // Multi-pass: process only one light, provided by u_light_index
    int i = u_light_index;

    if (i == 0) {
        ambient = u_ambient_color * color.rgb;
    }

    vec3 L_unnorm = u_light_pos[i] - v_world_position;
    vec3 L;
    float attenuation = 1.0;
    float d = length(L_unnorm);

    // Light direction and attenuation
    if (u_light_type[i] == 1) {
        L = normalize(L_unnorm);
        d = max(d, 0.01);
        attenuation = 1.0 / (d * d);
    } else if (u_light_type[i] == 3) {
        L = normalize(-normalize(u_light_dir[i]));
        attenuation = 1.0;
    } else if (u_light_type[i] == 2) {
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

    total_diff += attenuation * diff * light_color;
    total_spec += attenuation * spec * u_specular_strength * light_color;

    vec3 final_color = ambient + total_diff * color.rgb + total_spec;
    FragColor = vec4(N, color.a);
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
	float z = texture(u_texture,v_uv).x;
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


\volume.fs
#version 330 core

// Fragment Shader
in vec3 v_world_position;
out vec4 FragColor;

uniform sampler2D u_gbuffer_albedo;
uniform sampler2D u_gbuffer_normals;
uniform sampler2D u_gbuffer_depth;

uniform mat4 u_inverse_viewprojection;
uniform vec2 u_iResolution;
uniform vec3 u_camera_position;

uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform int u_light_type;
uniform vec3 u_light_dir;
uniform vec2 u_light_cone;

vec3 getWorldPosition(vec2 uv, float depth)
{
    vec4 pos = u_inverse_viewprojection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return pos.xyz / pos.w;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * u_iResolution;
    float depth = texture2D(u_gbuffer_depth, uv).x;
    
    // Early exit if no geometry
    if (depth == 1.0) discard;
    
    vec3 world_pos = getWorldPosition(uv, depth);
    vec3 albedo = texture2D(u_gbuffer_albedo, uv).rgb;
    vec3 normal = texture2D(u_gbuffer_normals, uv).xyz * 2.0 - 1.0;
    
    // Vector luz -> superficie
    vec3 L = u_light_pos - world_pos;
    float dist = length(L);
    L = normalize(L);
    
    // Atenuación
    float att = 1.0 / (1.0 + dist * dist);
    
    // Spot light factor
    if (u_light_type == 2) // SPOT
    {
        float cos_angle = dot(-L, u_light_dir);
        float spot = smoothstep(u_light_cone.y, u_light_cone.x, cos_angle);
        att *= spot;
    }
    
    // Diffuse
    float NdotL = max(0.0, dot(normal, L));
    vec3 diffuse = albedo * NdotL * att * u_light_color;
    
    FragColor = vec4(diffuse, 1.0);
}


\fire.vs
#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

uniform mat4 u_model;
uniform mat4 u_viewprojection;
uniform float u_time;

out vec2 v_uv;
out float v_distort;

void main()
{
    // Pass UVs to fragment shader
    v_uv = a_coord;

    // Animate the vertex position for fire effect (simple vertical distortion)
    float distortion = sin(a_vertex.x * 10.0 + u_time * 2.0) * 0.05;
    v_distort = distortion;

    vec3 pos = a_vertex;
    pos.y += distortion;

    // Transform to world and clip space
    vec4 world_pos = u_model * vec4(pos, 1.0);
    gl_Position = u_viewprojection * world_pos;
}

\fire.fs
#version 330 core

in vec2 v_uv;
in float v_distort;

out vec4 FragColor;

uniform sampler2D u_fire_texture;
uniform float u_time;
uniform float u_intensity;

void main()
{
    // Animate UVs for a moving fire effect
    vec2 uv = v_uv;
    uv.y += u_time * 0.5;
    uv.x += sin(u_time + v_uv.y * 5.0) * 0.02;

    // Add distortion from vertex shader
    uv.y += v_distort * 2.0;

    // Sample the fire texture
    vec4 fireColor = texture(u_fire_texture, uv);

    // Optional: boost intensity and fade out low alpha
    fireColor.rgb *= u_intensity;
    if (fireColor.a < 0.05)
        discard;

    FragColor = fireColor;
}