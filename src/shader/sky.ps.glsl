uniform sampler2D tex;
void main(){
    vec2 t = gl_TexCoord[0].st;
    t = vec2(t.x,1.0-t.y);
    vec4 color = texture2D(tex,t);
    gl_FragData[0] = color;
    gl_FragData[3] = vec4(vec3(0.0),1.0);
}
