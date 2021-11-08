#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;  

const int NUM_CAMERA = 24;

uniform vec3 ViewPos; 
vec3 lightPos = vec3(0.0, 0.0, 2.0);
vec3 lightColor = vec3(1.0, 1.0, 1.0);

vec3 CalcPointLight(vec3 lightPos);

void main()
{
	vec3 result = CalcPointLight(lightPos);
    FragColor = vec4(result, 1.0);
} 

vec3 CalcPointLight(vec3 lightPos)
{
	  // ambient
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.8;
    vec3 viewDir = normalize(ViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * vec3(0.8, 0.8, 0.8);
	  return result;
}