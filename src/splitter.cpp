#if _MSC_VER
#define NO_INTRINSICS 1
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "../libberdip/src/common.h"
#include "../libberdip/src/maps.h"
#include "../libberdip/src/maths.h"
#include "../libberdip/src/strings.h"
#include "../libberdip/src/files.h"

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        f32 epsilon = 0.001f;
        String ratioStr = string(argv[1]);
        
        u32 colonIndex = 0;
        while ((colonIndex < ratioStr.size) &&
               (ratioStr.data[colonIndex] != ':'))
        {
            ++colonIndex;
        }
        
        if (argc > 2)
        {
            f32 newEps = float_from_string(string(argv[2]));
            if (newEps > 0.0f)
            {
                epsilon = newEps;
            }
            else
            {
                fprintf(stderr, "Epsilon value out of range (%f), continuing with %f\n", newEps, epsilon);
            }
        }
        
        if (colonIndex < ratioStr.size)
        {
            String inputRateStr = {colonIndex, ratioStr.data};
            String outputRateStr = {ratioStr.size - colonIndex - 1, ratioStr.data + colonIndex + 1};
            
            if (inputRateStr.size)
            {
                if (outputRateStr.size)
                {
                    f64 inputRate = float_from_string(inputRateStr);
                    f64 outputRate = float_from_string(outputRateStr);
                    
                    fprintf(stdout, "In: %f, Out: %f\n", inputRate, outputRate);
                    
                }
                else
                {
                    fprintf(stderr, "ERROR: Missing output in ratio!\nUsage: %s <ratio input:output> [epsilon 0.001]\n", argv[0]);
                }
            }
            else
            {
                fprintf(stderr, "ERROR: Missing input in ratio!\nUsage: %s <ratio input:output> [epsilon 0.001]\n", argv[0]);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Missing colon in ratio!\nUsage: %s <ratio input:output> [epsilon 0.001]\n", argv[0]);
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s <ratio input:output> [epsilon 0.001]\n", argv[0]);
    }
    
    return 0;
}

/*
digraph "splitting" {
  splines = false
  inA [shape=record, label="{100%|{<a>33.3%|<b>33.3%|<c>33.3%}}"]
  inB [shape=record, label="{100%|{<a>33.3%|<b>33.3%|<c>33.3%}}"]
  outA [shape=record, label="{{<a>33.3%|<b>|<c>33.3%}|66.7%}"]
  outB [shape=record, label="{{<a>33.3%|<b>|<c>33.3%}|66.7%}"]
  outC [shape=record, label="{{<a>33.3%|<b>|<c>33.3%}|66.7%}"]
  inA:a -> outA:a
  inA:b -> outB:a
  inA:c -> outC:a
  inB:a -> outA:c
  inB:b -> outB:c
   inB:c -> outC:c
}
*/



