
uniform sampler2D v_texture;
uniform float pixellation;
uniform vec2 aspectRatio;

in vec2 v_texCoord;

out vec4 fragColor;
void main() { 
    if (pixellation > 1) {
        vec2 uv = v_texCoord / pixellation;
        fragColor = texture(v_texture, uv);
    } else {
        fragColor = texture(v_texture, v_texCoord);
    }
    
}
