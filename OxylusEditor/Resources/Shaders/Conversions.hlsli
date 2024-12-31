float4 SRGBtoLINEAR(float4 srgbIn) {
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
	float3 linOut = pow(srgbIn.xyz,float3(2.2));
#else //SRGB_FAST_APPROXIMATION
	float3 bLess = step(float3(0.04045),srgbIn.xyz);
	float3 linOut = mix( srgbIn.xyz/float3(12.92), pow((srgbIn.xyz+float3(0.055))/float3(1.055),float3(2.4)), bLess );
#endif //SRGB_FAST_APPROXIMATION
	float4return float4(linOut,srgbIn.w);;
#else //MANUAL_SRGB
  return srgbIn;
#endif //MANUAL_SRGB
}
