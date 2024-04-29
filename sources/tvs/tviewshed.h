/**
 * @file main.h
 * @author Felipe Romero
 * @brief Other main functions. Separated into a header to make the code more readable.
 * @date 2022-11-17
 */

#define STEP 30
#define OBS_H 1.5
#define USE_OPENCL

bool verbose=true;

enum RUN_MODE{CPU_MODE,GPU_MODE,HYBRID_MODE};
enum GPU_MODE{CUDA_MODE,OPENCL_MODE};
enum KERNELS{KERNEL_SDEM, KERNEL_IDENTITYDEM};


typedef struct  {
    float min;
    float max;
}    pair_t;

template <typename T>
struct inputData{
    T *input0;
    T *input1;
    T *input2;
    T *input3;
};
inputData<float> inData;

float *outD;
unsigned char *outH;
unsigned char **horizontes=new unsigned char *[90];

int runMode=HYBRID_MODE; // 0 cpu, 1 gpu, 2 hybrid
int gpuMode=OPENCL_MODE; // 0 cuda, 1 opencl

int nthreads = -1;//not set
int nCPUs,nGPUs;
int dim,dimx,dimy,N;  //N and dim are aliases
int nSectors=360;
float step=STEP; // Grid size
float POVh=OBS_H; // Elevación en scala step
float surScale; // Escala en superficie
float maxDistance=0;
int maskPOVs=0;
int outputType=0; // viewshed surface (0), mask (1), horizon (2)...

std::vector<cl::Device> OCLDevices;

#include "skewEngine.h"

/*************************************************************************************************************/
//
/*************************************************************************************************************/

pair_t getMinMax(float * datos)
{
    pair_t resultado;
    resultado.min=FLT_MAX;
    resultado.max=FLT_MIN;
    for(int i=0;i<dimx*dimy;i++)
    {
        resultado.min=datos[i]<resultado.min?datos[i]:resultado.min;
        resultado.max=datos[i]>resultado.max?datos[i]:resultado.max;
    }
    return resultado;
}

void openCLcapabilities(int *n_GPUs)
{
    //A platform is a specific OpenCL implementation, for instance AMD APP, NVIDIA or Intel OpenCL.
    // A context is a platform with a set of available devices for that platform.
    // And the devices are the actual processors (CPU, GPU etc.) that perform calculations.
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    auto platform = platforms[0];
    platform.getDevices(CL_DEVICE_TYPE_GPU, &OCLDevices);
    size_t nCLGPUS=OCLDevices.size();
    printf("%d OCL-enabled GPUS has been found\n",nCLGPUS);
    *n_GPUs=(int)nCLGPUS;

}

void setResources()
{
    if(runMode==HYBRID_MODE)runMode=GPU_MODE;
    //Veamos si hay gpus
    nGPUs = 0;
    if(gpuMode==OPENCL_MODE)
        openCLcapabilities(&nGPUs);

    if((runMode==HYBRID_MODE||runMode==GPU_MODE)&&nGPUs==0)runMode=CPU_MODE;

    nCPUs=omp_get_num_procs();
    if(runMode==CPU_MODE)nGPUs=0;
    if(nthreads==-1)
    {
        nthreads=nCPUs;
        while((180%nthreads))nthreads--;
        printf("nCPUs (trimmed to load balance):  %d\n",nthreads);
    }


    if(runMode==GPU_MODE)nthreads=nGPUs;
    //What happens in runmode 2? Not set
    //omp_set_num_threads(nthreads);
    if(verbose) {
//        nthreads=omp_get_num_threads();
        printf("%d CPUs and %d GPUs found. nthreads set to %d\n", nCPUs, nGPUs, nthreads);
    }
    fflush(stdout);
}

void execute(GDALProgressFunc pfnProgress,void *pProgressArg)
{
//inData is the input data
//inputD is a struct with 4 pointers (4 arrays of input data: NN, TM, TN, TM)

omp_set_num_threads(nthreads);
#pragma omp parallel default(shared)
    {
        int id = omp_get_thread_num();
        //if(id==0 && verbose)printf("Actual number of threads: %d\n",omp_get_num_threads());fflush(stdout);

// Each thread (in CPU mode -> arbitrary)  (in GPU mode -> nthreads = num of GPUs) has its own engine:
        skewEngine<float> *skewer=new skewEngine<float>(dimx, dimy, static_cast<inputData<float>>(inData), runMode == GPU_MODE,id);
        skewer->useEarthCurve=true;
        skewer->step=step;
        skewer->maxDistance=maxDistance;
        skewer->maskPOVs=maskPOVs;
        skewer->outputType = outputType;
        skewer->POVh=POVh; //Observer's height in step-size unit
        skewer->nSectors=nSectors;

        if(outputType==TVS_HORIZON)
            skewer->horizontes=horizontes;


        //Dentro del constructor? ->
        cl::Context OCLContext;
        cl::Device OCLDevice;
        if(runMode==GPU_MODE && gpuMode == OPENCL_MODE) {
            skewer->isCUDA=false;
            OCLDevice=OCLDevices[id];
            skewer->OCLDevice =OCLDevice;
            OCLContext=cl::Context(OCLDevice);
            skewer->OCLContext = OCLContext;
            cl::CommandQueue OCLQueue;
            OCLQueue= cl::CommandQueue(OCLContext,OCLDevice);
            skewer->OCLQueue = OCLQueue;
        }


        skewer->skewAlloc();




#pragma omp barrier
#pragma omp for schedule(dynamic) nowait
        for (int i = 0; i < nSectors/2; i++) {

            skewer->skew(i);

            skewer->kernel();

            skewer->deskew();

            if(id==0)pfnProgress((i+1)/(nSectors/2.0), "", pProgressArg);

        }

#pragma omp critical
        {
            // When finishing, thread data are added to outD
            skewer->reduce((void *)outD);
        }
        delete skewer;


    } //end parallel region

if(verbose)printf("Exit from parallel region\n");

}

