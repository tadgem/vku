Reconstruct position from depth:

```glsl
float z = texture(depthTexture, texCoord).x;
vec2 clipSpaceTexCoord = texCoord;
vec4 clipSpacePosition = vec4(texCoord * 2.0 - 1.0, z, 1.0);
vec4 viewSpacePosition = projectionInverse * clipSpacePosition;
viewSpacePosition /= viewSpacePosition.w;   
vec3 WorldPos = (viewInverse * viewSpacePosition).xyz;
```

Reuse already created pipelines:
[Pipeline Cache :: Vulkan Documentation Project](https://docs.vulkan.org/guide/latest/pipeline_cache.html)