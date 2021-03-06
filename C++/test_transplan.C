#include "p3dfft.h"
//#include "defs.h"
#include <math.h>
#include <stdio.h>
//#include "templ.C"
//#include "exec.C"

using namespace p3dfft;

void init_wave1D(double *,int[3],int *,int[3],int,int[3]);
void print_res(double *,int *,int *,int *, int *,int);
void normalize(double *,long int,int *,int);
double check_res(double*,double *,int *, int *);

main(int argc,char **argv)
{

using namespace p3dfft;

  int N=64;
  int Nrep = 1;
  int myid,nprocs;
  int gdims[3],gdims2[3];
  int pgrid1[3],pgrid2[3];
  int proc_order[3];
  int mem_order[3];
  int mem_order2[3];
  int i,j,k,x,y,z,p1,p2;
  double Nglob;
  int imo1[3];
  //  void inv_mo(int[3],int[3]);
  //void write_buf(double *,char *,int[3],int[3],int);
  int *ldims,*ldims2;
  long int size1,size2;
  double *IN;
  int *glob_start,*glob_start2;
  double *OUT;
  // Set up transform types for 3D transform
  int type_ids1;
  int type_ids2;
  Type3D type_rcc,type_ccr;
  double t=0.;
  double *FIN;
  double mydiff;
  double diff = 0.0;
  double gtavg=0.;
  double gtmin=INFINITY;
  double gtmax = 0.;
  int pdims[2],nx,ny,nz,n,dim,cnt;
  FILE *fp;

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);

   if(myid == 0) {
     printf("P3DFFT++ test1. Running on %d cores\n",nprocs);
     if((fp=fopen("trans.in", "r"))==NULL){
        printf("Cannot open input file. Setting to default nx=ny=nz=128, dim=0, n=1.\n");
        nx=ny=nz=128; Nrep=1;dim=0;
     } else {
        fscanf(fp,"%d %d %d %d %d\n",&nx,&ny,&nz,&dim,&Nrep);
        fscanf(fp,"%d %d %d\n",mem_order,mem_order+1,mem_order+2);
        fscanf(fp,"%d %d %d\n",mem_order2,mem_order2+1,mem_order2+2);
        fclose(fp);
     }
     printf("P3DFFT test, 1D wave input, 1D FFT\n");
#ifndef SINGLE_PREC
     printf("Double precision\n (%d %d %d) grid\n dimension of transform: %d\n%d repetitions\n",nx,ny,nz,dim,n);
#else
     printf("Single precision\n (%d %d %d) grid\n dimension of transform %d\n%d repetitions\n",nx,ny,nz,dim,n);
#endif
   }
   MPI_Bcast(&nx,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&ny,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&nz,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&Nrep,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&dim,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&mem_order,3,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&mem_order2,3,MPI_INT,0,MPI_COMM_WORLD);

  //! Establish 2D processor grid decomposition, either by readin from file 'dims' or by an MPI default

     fp = fopen("dims","r");
     if(fp != NULL) {
       if(myid == 0)
         printf("Reading proc. grid from file dims\n");
       fscanf(fp,"%d %d\n",pdims,pdims+1);
       fclose(fp);
       if(pdims[0]*pdims[1] != nprocs)
          pdims[1] = nprocs / pdims[0];
     }
     else {
       if(myid == 0)
          printf("Creating proc. grid with mpi_dims_create\n");
       pdims[0]=pdims[1]=0;
       MPI_Dims_create(nprocs,2,pdims);
       if(pdims[0] > pdims[1]) {
          pdims[0] = pdims[1];
          pdims[1] = nprocs/pdims[0];
       }
     }

   if(myid == 0)
      printf("Using processor grid %d x %d\n",pdims[0],pdims[1]);

  // Set up work structures for P3DFFT

  setup();

  //Set up 2 transform types for 3D transforms

  type_ids1 = R2CFFT_D;
  type_ids2 = C2RFFT_D;

  //Set up global dimensions of the grid

  gdims[0] = nx;
  gdims[1] = ny;
  gdims[2] = nz;
  //  mem_order[dim] = 0;
  cnt = 1;
  for(i=0; i < 3;i++) {
    //    gdims[i] = N;
    proc_order[i] = i;
    // if(i != dim)
    //  mem_order[i] = cnt++;
  }

  //  p1 = floor(sqrt(nprocs));
  //p2 = nprocs / p1;
  p1 = pdims[0];
  p2 = pdims[1];

  // Define the initial processor grid. In this case, it's a 2D pencil, with 1st dimension local and the 2nd and 3rd split by iproc and jproc tasks respectively

  cnt=0;
  for(i=0;i<3;i++)
    if(i == dim)
      pgrid1[i] = 1;
    else
      pgrid1[i] = pdims[cnt++];

  // Set up the final global grid dimensions (these will be different from the original dimensions in one dimension since we are doing real-to-complex transform)

  for(i=0; i < 3;i++) 
    gdims2[i] = gdims[i];
  gdims2[dim] = gdims2[dim]/2+1;

  //Initialize initial and final grids, based on the above information

  //  printf("Initiating grid1\n");  
  grid grid1(gdims,pgrid1,proc_order,mem_order,MPI_COMM_WORLD); 

  //  printf("Initiating grid2\n");
  grid grid2(gdims2,pgrid1,proc_order,mem_order2,MPI_COMM_WORLD); 


  //Set up the forward transform, based on the predefined 3D transform type and grid1 and grid2. This is the planning stage, needed once as initialization.
  //  printf("Plan rcc\n");
  transplan<double,complex_double> trans_f(grid1,grid2,type_ids1,dim,0);

  //Now set up the backward transform

  //printf("Plan ccr\n");
  transplan<complex_double,double> trans_b(grid2,grid1,type_ids2,dim,0);

  //Determine local array dimensions. 

  ldims = grid1.ldims;
  size1 = ldims[0]*ldims[1]*ldims[2];
  //  printf("Allocating IN\n");

  //Now allocate initial and final arrays in physical space (real-valued)
  IN=(double *) malloc(sizeof(double)*size1);
  FIN= (double *) malloc(sizeof(double) *size1);

  //Initialize the IN array with a sine wave in 3D

  //  printf("Initiating wave\n");
  glob_start = grid1.glob_start;
  init_wave1D(IN,gdims,ldims,glob_start,dim,mem_order);

  //Determine local array dimensions and allocate fourier space, complex-valued out array

  ldims2 = grid2.ldims;
  glob_start2 = grid2.glob_start;
  size2 = ldims2[0]*ldims2[1]*ldims2[2];
  //  printf("allocating OUT, size=%d\n",size2);
  OUT=(double *) malloc(sizeof(double) *size2 *2);

  trans_f.exec((char *) IN,(char *) OUT);

  Nglob = gdims[0]*gdims[1]*gdims[2];

  if(myid == 0)
    printf("Results of forward transform: \n");
  print_res(OUT,gdims,ldims2,glob_start2,mem_order2,dim);
  normalize(OUT,ldims2[0]*ldims2[1]*ldims2[2],gdims,mem_order2[dim]);
  trans_b.exec((char *) OUT,(char *) FIN);

  mydiff = check_res(IN,FIN,ldims,mem_order);
  printf("%d: my diff =%lf\n",myid,mydiff);
  diff = 0.;
  MPI_Reduce(&mydiff,&diff,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  if(myid == 0) {
    if(diff > 1.0e-14 * Nglob *0.25)
      printf("Results are incorrect\n");
    else
      printf("Results are correct\n");
    printf("Max. diff. =%lf\n",diff);
  }

  free(IN); free(OUT); free(FIN);

  // Clean up grid structures

  // Clean up all P3DFFT++ data

  p3dfft_cleanup();

  MPI_Finalize();
}

void normalize(double *A,long int size,int *gdims,int dim)
{
  long int i;
  double f = 1.0/(((double) gdims[dim]));
  
  for(i=0;i<size*2;i++)
    A[i] = A[i] * f;

}

void init_wave1D(double *IN,int *gdims,int *ldims,int *gstart, int dim, int mem_order[3])
{
  double *mysin,*p;
  int i,x,y,z,ld,d[3];
  double twopi = atan(1.0)*8.0;

  mysin = (double *) malloc(sizeof(double)*gdims[dim]);

  for(x=0;x < gdims[dim];x++)
    mysin[x] = sin(x*twopi/gdims[dim]);

  for(i=0;i<3;i++)
    d[mem_order[i]] = ldims[i];

   ld = mem_order[dim];
   p = IN;
   switch(ld) {
   case 0:

     for(z=0;z < d[2];z++)
       for(y=0;y < d[1];y++) 
	 for(x=0;x < d[0];x++)
	   *p++ = mysin[x];
       
     break;

   case 1:

     for(z=0;z < d[2];z++)
       for(y=0;y < d[1];y++) 
	 for(x=0;x < d[0];x++)
	   *p++ = mysin[y];
       
     break;
     
   case 2:

     for(z=0;z < d[2];z++)
       for(y=0;y < d[1];y++) 
	 for(x=0;x < d[0];x++)
	   *p++ = mysin[z];
       
     break;
   default:
     break;
   }

   free(mysin); 
}

void print_res(double *A,int *gdims,int *ldims,int *gstart, int *mo, int dim)
{
  int x,y,z;
  double *p;
  double N;
  int imo[3],i,j;
  
  for(i=0;i<3;i++)
    for(j=0;j<3;j++)
      if(mo[i] == j)
	imo[j] = i;

  N = gdims[mo[dim]];
  p = A;
  for(z=0;z < ldims[imo[2]];z++)
    for(y=0;y < ldims[imo[1]];y++)
      for(x=0;x < ldims[imo[0]];x++) {
	if(fabs(*p) > N *1.25e-4 || fabs(*(p+1))  > N *1.25e-4) 
    printf("(%d %d %d) %lg %lg\n",x+gstart[imo[0]],y+gstart[imo[1]],z+gstart[imo[2]],*p,*(p+1));
	p+=2;
      }
}

double check_res(double *A,double *B,int *ldims, int *mo)
{
  int x,y,z;
  double *p1,*p2,mydiff;
  int imo[3],i,j;
  p1 = A;
  p2 = B;
  
  for(i=0;i<3;i++)
    for(j=0;j<3;j++)
      if(mo[i] == j)
	imo[j] = i;

  mydiff = 0.;
  for(z=0;z < ldims[imo[2]];z++)
    for(y=0;y < ldims[imo[1]];y++)
      for(x=0;x < ldims[imo[0]];x++) {
	if(fabs(*p1 - *p2) > mydiff)
	  mydiff = fabs(*p1-*p2);
	p1++;
	p2++;
      }
  return(mydiff);
}

void write_buf(double *buf,char *label,int sz[3],int mo[3], int taskid) {
  int i,j,k;
  FILE *fp;
  char str[80],filename[80];
  double *p= buf;

  strcpy(filename,label);
  sprintf(str,".%d",taskid);
  strcat(filename,str);
  fp=fopen(filename,"w");
  for(k=0;k<sz[mo[2]];k++)
    for(j=0;j<sz[mo[1]];j++)
      for(i=0;i<sz[mo[0]];i++) {
	if(abs(*p) > 1.e-7) {
	  fprintf(fp,"(%d %d %d) %lg\n",i,j,k,*p);
	}
	p++;
      }
  fclose(fp); 
}
