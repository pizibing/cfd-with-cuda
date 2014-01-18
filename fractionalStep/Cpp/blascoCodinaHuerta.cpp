//
// Based on Backup 3
//
// For NE=27 mesh, running with F5 in Release mode fails.
//
// 3D MATLAB kodunu 01-10-2013'de C++'a cevirmeye basladim.
// 25-12-2013'de C++'dan ilk Cavity cevabini aldim.
//
//


/*****************************************************************
*        This code is a part of the CFD-with-CUDA project        *
*             http://code.google.com/p/cfd-with-cuda             *
*                                                                *
*              Dr. Cuneyt Sert and Mahmut M. Gocmen              *
*                                                                *
*              Department of Mechanical Engineering              *
*                Middle East Technical University                *
*                         Ankara, Turkey                         *
*                 http://www.metu.edu.tr/~csert                  *
*                                                                *
*****************************************************************/

#include <stdio.h>

#ifdef WIN32
   #include <time.h>
#else
   #include <sys/time.h>
   #define CLOCKS_PER_SEC 1.0
#endif // WIN32

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <vector>
#include "mkl_types.h"
#include "mkl_spblas.h"
//#include <mkl.h>
//#include <cusp/coo_matrix.h>
//#include <cusp/multiply.h>
//#include <cusp/print.h>
//#include <cusp/io/matrix_market.h>
//#include <cusp/graph/symmetric_rcm.h>

// #include <cusparse.h>
// #include <cublas.h>

extern "C" {      // Timothy Davis' CSparse library
#include "cs.h"
}

using namespace std;

#ifdef SINGLE              // Many major parameters can automatically be defined as                                                                  TODO: This is not used in the code.
  typedef float real;      // float by using -DSINGLE during compilcation. Default
#else                      // behavior is to use double precision.
  typedef double real;
#endif




//========================================================================
// Global Variables
//========================================================================

ifstream inpFile;           // Input file with INP extension.
ifstream problemNameFile;   // Input file name is read from this ProblemName.txt file.
ofstream datFile;           // Output file with DAT extension.
//ofstream outputControl;     // Used for debugging.
//ofstream outFile;           // Output file to examine results.

string whichProblem;
//string problemName      = "ProblemName.txt";
//string controlFile      = "Control_Output.txt";
//string outputExtension;
//string outputExtensionPostProcess;
//string restartExtension = "_restart.dat";

int eType;   // Element type. 1: 3D Hexahedron, 2: 3D Tetrahedron
int NE;      // Number of elements
int NN;      // Number of all nodes
int NCN;     // Number of corner nodes (no mid-edge, mid-face or mid-element nodes)
int NNp;     // Number of pressure nodes
int NENv;    // Number of velocity nodes on an element
int NENp;    // Number of pressure nodes on an element
int NGP;     // Number of Gauss Quadrature points
//int NEU;   // Number of elemental unknowns, 3*NENv + NENp
//int Ndof;  // Total number of unknowns in the problem

double dt;         // Time step
double t_ini;      // Initial time
double t_final;    // Final time
int timeN;         // Discrete time level
double timeT;      // Actual time

int    maxIter;    // Maximum iteration for step 3 of the solution.
double tolerance;  // tolerance for the iterative solution used in step 3

bool   isRestart;  // Switch that defines if solver continues from a previous
                   // solution or starts from the initial condition
double density;    // Density of the material
double viscosity;  // Viscosity of the material                                                                                                      TODO: Kinematic or dynamic?
double fx, fy, fz; // Body force components

double alpha;      //                                                                                                                                TODO : Read from the input file, but not used

int    NEC;        // Number of element corners
int    NEF;        // Number of element faces
int    NEE;        // Number of element edges

double **coord;    // Coordinates (x, y, z) of mesh nodes. Initial size is [NE*NENv][3]. Later reduces to [NN][3]

int **LtoGnode;    // Local to global node mapping of velocity nodes (size:NExNENv)
int **LtoGvel;     // Local to global mapping of velocity unknowns (size:NEx3*NENv)
int **LtoGpres;    // Local to global mapping of pressure unknowns (size:NExNENp)

int **elemNeighbors;   // Neighbors of each element
int *NelemNeighbors;   // Number of neighbors of each element

int nBC;           // Number of different boundary conditions.
double *BCtype;    // Type of each BC. 1: Specified velocity
double **BCstr;    // Specified velocity values for each BC.                                                                                         TODO: These should actually be strings.
int BCnVelFaces;   // Number of element faces where velocity BC is specified.
int BCnVelNodes;   // Number of nodes where velocity BC is specified.
int BCnOutFaces;   // Number of element faces where outflow BC is specified.
int **BCvelFaces;  // Stores element number, face number and BC number for each face where velocity BC is specified.
int **BCvelNodes;  // Stores node number and BC number for each node where velocity BC is specified.
int **BCoutFaces;  // Stores element number, face number and BC number for each face where outflow BC is specified.
int zeroPressureNode;   // Node where pressure is set to zero. A negative value is ignored.

double monPointCoord[3];   // Coordinates of the monitor point.
int monPoint;              // Node that is being monitored.

int ** elemsOfVelNodes;    // List of elements that are connected to velocity nodes
int **elemsOfPresNodes;    // List of elements that are connected to pressure nodes
int *NelemOfVelNodes;      // Number of elements connnected to velocity nodes
int *NelemOfPresNodes;     // Number of elements connnected to pressure nodes


int sparseM_NNZ;        // Counts nonzero entries in i) a single sub-mass matrix and ii) full Mass matrix.
double *sparseMvalue;   // Nonzero values of the global mass matrix.
int *sparseMcol;        // Nonzero columns of M, K and A matrices.
int *sparseMrow;        // Nonzero rows of M, K and A matrices.
int *sparseMrowIndex;   // Row start indices of M, K and A matrices (for CSR storage).

double *sparseKvalue;   // Nonzero values of the global K matrix. K has the same sparsity structure as M.
double *sparseAvalue;   // Nonzero values of the global A matrix. A has the same sparsity structure as M.

int sparseG_NNZ;        // Counts nonzero entries in i) sub-G matrix and ii) full G matrix.
double *sparseGvalue;   // Nonzero values of the global G matrix.
int *sparseGcol;        // Nonzero columns of G matrix.
int *sparseGrow;        // Nonzero rows of G matrix.
int *sparseGrowIndex;   // Row start indices of G matrix (for CSR storage).

cs *G_cs, *G_cs_CSC;    // CSparse storage of [G]
cs *Gt_cs_CSC;          // CSparse storage of [G] transpose
//cs *K_cs, *K_cs_CSC;    // CSparse storage of [K]
//cs *A_cs, *A_cs_CSC;    // CSparse storage of [A]


double *sparseMdOrigInvTimesKvalue;  // Values of inv(Md) * K sparse matrix.

cs *Z_cs;               // [Z] matrix.
css *Z_sym;             // Symbolic analysis of [Z] using the CSparse library. Will be calculated in step0, and used in step2.
csn *Z_chol;            // Cholesky factorization of [Z] using the CSparse library. Will be calculated in step0, and used in step2.


int ***sparseMapM;      // Maps each element's local M, K, A entries to the global ones that are stored in sparse format.
int ***sparseMapG;      // Maps each element's local G entries to the global ones that are stored in sparse format.



double **GQpoint, *GQweight; // GQ points and weights.



double **Sp;       // Shape functions for pressure evaluated at GQ points. (size:NENpxNGP)
double ***dSp;     // Derivatives of shape functions for pressure wrt to ksi, eta & zeta evaluated at GQ points. (size:NENpxNGP)
double **Sv;       // Shape functions for velocity evaluated at GQ points. (size:NENvxNGP) 
double ***dSv;     // Derivatives of shape functions for velocity wrt to ksi, eta & zeta evaluated at GQ points. (size:NENvxNGP)



double **detJacob; // Determinant of the Jacobian matrix evaluated at a certain (ksi, eta, zeta)
double ****gDSp;   // Derivatives of shape functions for pressure wrt x, y & z at GQ points. (size:3xNENvxNGP)
double ****gDSv;   // Derivatives of shape functions for velocity wrt x, y & z at GQ points. (size:3xNENvxNGP)



double *Un;               // x, y and z velocity components of time step n.
double *Unp1;             // U_i+1^n+1 of the reference paper.
double *Unp1_prev;        // U_i^n+1 of the reference paper.
double *UnpHalf;          // U_i+1^n+1/2 of the reference paper.
double *UnpHalf_prev;     // U_i^n+1/2 of the reference paper.

double *AccHalf;          // A_i+1^n+1/2 of the reference paper.
double *Acc;              // A_i+1^n+1 of the reference paper.
double *Acc_prev;         // A_i^n+1 of the reference paper.

double *Pn;               // Pressure of time step n.
double *Pnp1;             // U_i+1^n+1 of the reference paper.
double *Pnp1_prev;        // p_i+1^n+1 of the reference paper.
double *Pdot;             // Pdot_i+1^n+1 of the reference paper.

double *Md;               // Diagonalized mass matrix with BCs applied
double *MdOrig;           // Diagonalized mass matrix without BCs applied
double *MdInv;            // Inverse of the diagonalized mass matrix with BCs applied
double *MdOrigInv;        // Inverse of the diagonalized mass matrix without BCs applied

double *R1;               // RHS vector of intermediate velocity calculation.
double *R2;               // RHS vector of pressure calculation.
double *R3;               // RHS vector of new velocity calculation.


//int nDATiter;        // Period of saving results.
//int nMonitorPoints;     // Number of monitor points.




//========================================================================
// Functions
//========================================================================
void readInputFile();
double getHighResolutionTime();
void findElemNeighbors();
void setupNonCornerNodes();
void setupLtoGdof();
void determineVelBCnodes();
void findElemsOfPresNodes();
void findElemsOfVelNodes();
void findMonitorPoint();
void setupSparseM();
void setupSparseG();
void setupGQ();
void calcShape();
void calcJacob();
void initializeAndAllocate();
void readRestartFile();
void createTecplot();
void timeLoop();
void step0();
void step1(int);
void step2();
void step3();
void applyBC_initial();
void applyBC_Step1(int);
void applyBC_Step2(int);
void applyBC_Step3();
void waitForUser(string);



/*
// Pressure correction equation solvers
#ifdef CG_CUDA
   extern void CUSP_pC_CUDA_CG();
#endif
#ifdef CR_CUDA
   extern void CUSP_pC_CUDA_CR();
#endif
#ifdef CG_CUSP
   extern void CUSP_pC_CUSP_CG();
#endif
#ifdef CR_CUSP
   extern void CUSP_pC_CUSP_CR();
#endif

// Momentum equation solvers
#ifdef GMRES_CUSP
   extern void CUSP_GMRES();
#endif
#ifdef BiCG_CUSP
   extern void CUSP_BiCG();
#endif

*/





//========================================================================
int main()
//========================================================================
{
   cout << "\n\n*********************************************************";
   cout << "\n*    3D Unsteady Incompressible Navier-Stokes Solver    *";
   cout << "\n*        Formulation of Blasco, Codina & Huerta         *";
   cout << "\n*            Part of CFD with CUDA project              *";
   cout << "\n*        http://code.google.com/p/cfd-with-cuda         *";
   cout << "\n*********************************************************\n\n";

   waitForUser("Just started. Enter a character... ");
   
   double Start, End, Start1, End1;       // Used for run time measurement.

   Start1 = getHighResolutionTime();   

   Start = getHighResolutionTime();
   readInputFile();                       // Read the input file.
   End = getHighResolutionTime();
   printf("readInputFile()        took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   findElemsOfPresNodes();                // Finds elements that are connected to each pressure node.
   End = getHighResolutionTime();
   printf("findElemsOfPresNodes() took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   findElemNeighbors();                   // Finds neighbors of all elements.
   End = getHighResolutionTime();
   printf("findElemNeighbors()    took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   setupNonCornerNodes();                 // Find non-corner nodes, add them to LtoGnode and calculate their coordinates.
   End = getHighResolutionTime();
   printf("setupNonCornerNodes()  took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   setupLtoGdof();                        // Creates LtoGvel and LtoGpres using LtoGnode.
   End = getHighResolutionTime();
   printf("setupLtoGdof()         took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   determineVelBCnodes();                 // Converts face-based velocity BC data into a node-based format.
   End = getHighResolutionTime();
   printf("determineVelBCnodes()  took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   findElemsOfVelNodes();                 // Finds elements that are connected to each velocity node.
   End = getHighResolutionTime();
   printf("findElemsOfVelNodes()  took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   findMonitorPoint();                    // Finds the node that is closest to the monitor point coordinates.
   End = getHighResolutionTime();
   printf("findMonitorPoint()     took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   Start = getHighResolutionTime();
   setupSparseM();                        // Finds the sparsity pattern of the Mass matrix.
   End = getHighResolutionTime();
   printf("setupSparseM()         took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   setupSparseG();                        // Finds the sparsity pattern of the G matrix.
   End = getHighResolutionTime();
   printf("setupSparseG()         took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   setupGQ();                             // Sets up GQ points and weights.
   End = getHighResolutionTime();
   printf("setupGQ()              took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   calcShape();                           // Calculates shape functions and their derivatives at GQ points.
   End = getHighResolutionTime();
   printf("calcShape()            took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   Start = getHighResolutionTime();
   calcJacob();                           // Calculates the determinant of the Jacobian and global shape function derivatives at each GQ point.
   End = getHighResolutionTime();
   printf("calcJacob()            took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   //Start = getHighResolutionTime();
   timeLoop();                           // Main solution loop.
   //End = getHighResolutionTime();
   //printf("timeLoop()            took  %8.3f seconds.\n", End - Start);

   
   End1 = getHighResolutionTime();
   printf("\nTotal run            took  %8.3f seconds.\n", (End1 - Start1) / CLOCKS_PER_SEC);
   
   cout << endl << "The program is terminated successfully.\n\n\n";

   waitForUser("Enter a character... ");

   createTecplot();

   return 0;

} // End of function main()





//========================================================================
void readInputFile()                                                                                                                                 // TODO: Define loop counters i and j inside the loops.
//========================================================================
{
   // Read the input file with INP extension.
   
   string dummy, dummy2, dummy4, dummy5;
   int dummy3, i, j;

   problemNameFile.open(string("ProblemName.txt").c_str(), ios::in);   // This is file called ProblemName.txt . It includes the name of the input file.
   problemNameFile >> whichProblem;   // This is used to construct input file's name.
   problemNameFile.close();
   
   inpFile.open((whichProblem + ".inp").c_str(), ios::in);
     
   inpFile.ignore(256, '\n');   // Read and ignore the line
   inpFile.ignore(256, '\n');   // Read and ignore the line

   inpFile.ignore(256, ':');    inpFile >> eType;       inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> NE;          inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> NCN;         inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> NENv;        inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> NENp;        inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> NGP;         inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> alpha;       inpFile.ignore(256, '\n');                                                                   // TODO: alpha is not used
   inpFile.ignore(256, ':');    inpFile >> dt;          inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> t_ini;       inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> t_final;     inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> maxIter;     inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> tolerance;   inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> isRestart;   inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> density;     inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> viscosity;   inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> fx;          inpFile.ignore(256, '\n');
   inpFile.ignore(256, ':');    inpFile >> fy;          inpFile.ignore(256, '\n');                                                                   // TODO: Also read fz



   // Read corner node coordinates
   coord = new double*[NE*NENv];   // At this point we'll read the coordinates of only NCN corner nodes.
                                   // Later we'll add non-corner nodes to it. At this point we do NOT
                                   // know the total number of nodes. Therefore we use a large enough
                                   // number of NE*NENv. Later the size will be reduced to NN.
   for (i=0; i<NE*NENv; i++) {
      coord[i] = new double[3];
   }

   inpFile.ignore(256, '\n');   // Read and ignore the line
   inpFile.ignore(256, '\n');   // Read and ignore the line

   for (i=0; i<NCN; i++){
      inpFile >> dummy3 >> coord[i][0] >> coord[i][1] >> coord[i][2];
      inpFile.ignore(256, '\n');
   }




   if (eType == 1) {      // Hexahedral element
     NEC = 8;      // Number of element corners
     NEF = 6;      // Number of element faces
     NEE = 12;     // Number of element edges
   } else {               // Tetrahedral element
     NEC = 4;
     NEF = 4;
     NEE = 6;
   }




   // Read corner nodes of each element, i.e. LtoGnode
   LtoGnode = new int*[NE];
   for (i=0; i<NE; i++) {
      LtoGnode[i] = new int[NENv];
   }


   for (i=0; i<NE; i++) {
      for (j=0; j<NENv; j++) {
         LtoGnode[i][j] = -1;     //Initialize to -1
      }
   }


   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile.ignore(256, '\n'); // Read and ignore the line 

   for (int e = 0; e < NE; e++){
      inpFile >> dummy3;
      for (i = 0; i < NEC; i++){
         inpFile >> LtoGnode[e][i];
         LtoGnode[e][i] = LtoGnode[e][i] - 1;                              // MATLAB -> C++ index switch 
      }
      inpFile.ignore(256, '\n'); // Read and ignore the line 
   }




   // Read number of different BC types and details of each BC
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile.ignore(256, '\n'); // Read and ignore the line
   
   inpFile.ignore(256, ':');    inpFile >> nBC;       inpFile.ignore(256, '\n');
   
   // Allocate BCtype and BCstr
   BCtype = new double[nBC];

   BCstr = new double*[nBC];
   for (i=0; i<nBC; i++) {
      BCstr[i] = new double[3];
   }
   
   for (i = 0; i<nBC; i++){
      inpFile.ignore(256, ':');
      inpFile >> BCtype[i];

      inpFile >> BCstr[i][0];
      inpFile >> dummy;

      inpFile >> BCstr[i][1];
      inpFile >> dummy;

      inpFile >> BCstr[i][2];
      inpFile.ignore(256, '\n'); // Ignore the rest of the line
   }
   
   inpFile.ignore(256, '\n'); // Read and ignore the line 
   inpFile.ignore(256, ':');     inpFile >> BCnVelFaces;
   inpFile.ignore(256, '\n'); // Ignore the rest of the line
   
   inpFile.ignore(256, ':');     inpFile >> BCnOutFaces;
   inpFile.ignore(256, '\n'); // Ignore the rest of the line



   // Read velocity BCs
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile.ignore(256, '\n'); // Read and ignore the line
      
   if (BCnVelFaces != 0){
      BCvelFaces = new int*[BCnVelFaces];
      for (i = 0; i < BCnVelFaces; i++){
         BCvelFaces[i] = new int[3];
      }
      
      for (i = 0; i < BCnVelFaces; i++){
         inpFile >> BCvelFaces[i][0] >> BCvelFaces[i][1] >> BCvelFaces[i][2];
         BCvelFaces[i][0] = BCvelFaces[i][0] - 1;                              // MATLAB -> C++ index switch
         BCvelFaces[i][1] = BCvelFaces[i][1] - 1;                              // MATLAB -> C++ index switch
         BCvelFaces[i][2] = BCvelFaces[i][2] - 1;                              // MATLAB -> C++ index switch
         inpFile.ignore(256, '\n'); // Ignore the rest of the line
      }
   }

  // Read outflow BCs
   inpFile.ignore(256, '\n'); // Read and ignore the line  
   inpFile.ignore(256, '\n'); // Read and ignore the line
   
   if (BCnOutFaces != 0){
      BCoutFaces = new int*[BCnOutFaces];
      for (i = 0; i < BCnOutFaces; i++){
         BCoutFaces[i] = new int[3];
      }
      for (i = 0; i < BCnOutFaces; i++){
         inpFile >> BCoutFaces[i][0] >> BCoutFaces[i][1] >> BCoutFaces[i][2];
         BCoutFaces[i][0] = BCoutFaces[i][0] - 1;                              // MATLAB -> C++ index switch
         BCoutFaces[i][1] = BCoutFaces[i][1] - 1;                              // MATLAB -> C++ index switch
         BCoutFaces[i][2] = BCoutFaces[i][2] - 1;                              // MATLAB -> C++ index switch
         inpFile.ignore(256, '\n'); // Ignore the rest of the line
      }
   }
   
   
   
   // Read the node where pressure is taken to be zero
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile >> zeroPressureNode;
   zeroPressureNode = zeroPressureNode - 1;                                // MATLAB -> C++ index switch
   inpFile.ignore(256, '\n'); // Ignore the rest of the line
   
   
   
   // Read monitor point coordinates
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile.ignore(256, '\n'); // Read and ignore the line
   inpFile >> monPointCoord[0] >> monPointCoord[1] >> monPointCoord[2];
   
   
   inpFile.close();
   
   
   // Determine NNp, number of pressure nodes
   if (NENp == 1) {   // Only 1 pressure node at the element center. Not tested at all.                                                              TODO: Either test this element and fully support it or remove details about it.
      NNp = NE;
   } else {
      NNp = NCN;      // Pressure are stored at element corners.
   }

   // CONTROL
   cout << endl << "NNp = " << NNp << endl;
   
} // End of function readInputFile()





//========================================================================
void findElemsOfPresNodes()
//========================================================================
{
   // Determines elements connected to pressure nodes. It is stored in a
   // matrix of size NNpx10, where 10 is a number, estimated to be larger
   // than the maximum number of elements connected to a pressure node.

   // Also an array (NelemOfPresNodes) store the actual number of elements
   // connected to each pressure node.

   // It is assumed that pressure nodes are at element corners.

   int LARGE = 10;  // It is assumed that not more than 10 elements are
                    // connected to a pressure node.
                                                                                                                                                     // TODO: Define this somewhere else, which will be easy to notice.
                                                                                                                                                     // TODO: Make sure that this is not violated.
   int node;

   elemsOfPresNodes = new int*[NNp];
   for (int i=0; i<NNp; i++) {
      elemsOfPresNodes[i] = new int[LARGE];
   }

   NelemOfPresNodes = new int[NNp];

   for (int i = 0; i < NNp; i++) {
      NelemOfPresNodes[i] = 0;      // Initialize to zero
   }

   // Form elemsOfPresNodes using LtoGnode of each element
   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENp; i++) {
         node = LtoGnode[e][i];   // It is assumed that pressure nodes are at element corners.
         elemsOfPresNodes[node][NelemOfPresNodes[node]] = e;
         NelemOfPresNodes[node] = NelemOfPresNodes[node] + 1;
      }
   }

   //  CONTROL
   //for (int i=0; i<NNp; i++) {
   //   cout << i << ":  " << NelemOfPresNodes[i] << endl;
   //}
   
   //for (int i=0; i<NNp; i++) {
   //   cout << i << ":  " ;
   //   for (int j=0; j<NelemOfPresNodes[i]; j++) {
   //      cout << elemsOfPresNodes[i][j] << "  ";
   //   }
   //   cout << endl;
   //}

}  // End of function findElemsOfPresNodes()





//========================================================================
void findElemNeighbors()
//========================================================================
{
   // Determines neighboring element/face for each face of each element.

   int node, elem;
   int LARGE = 26;                                                                                                                                   // TODO: 26 is the maximum number of elements for hexahedral elements. For tetrahedra's it'll be different
   bool inList;
   
   NelemNeighbors = new int[NE];
   elemNeighbors = new int*[NE];
   for (int i = 0; i < NE; i++) {
      elemNeighbors[i] = new int[LARGE];
   }

   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < LARGE; i++) {
         elemNeighbors[e][i] = -1;   // Initialize
      }
      NelemNeighbors[e] = 0;         // Initialize

      // Determine all elements around this element from elemsOfPresNodes
      for (int i = 0; i < NEC; i++) {
         node = LtoGnode[e][i];
         for (int j = 0; j < NelemOfPresNodes[node]; j++) {
            elem = elemsOfPresNodes[node][j];
            if (elem == e) {
               continue;
            }
            // Check if elem is already in allNeighbors list or not
            inList = 0;
            for (int k = 0; k < NelemNeighbors[e]; k++) {
               if (elem == elemNeighbors[e][k]) {
                  inList = 1;
                  break;
               }
            }
            if (inList == 0) {
               elemNeighbors[e][NelemNeighbors[e]] = elem;
               NelemNeighbors[e] = NelemNeighbors[e] + 1;
            }
         }
      }
   }  // End of element loop


   // CONTROL
   //for (int e = 0; e < NE; e++) {
   //   cout << e << ": " << NelemNeighbors[e] << ": ";
   //   for (int i = 0; i < NelemNeighbors[e]; i++) {
   //      cout << elemNeighbors[e][i] << ", ";
   //   }
   //   cout << endl;
   //}
   

}  // End of function findElemNeighbors()





//========================================================================
void setupNonCornerNodes()
//========================================================================
{

   // Calculates coordinates of non corner nodes and adds them to LtoGnode.

   if (NENv == NENp) {   // Don't do anything if NENv == NENp
      return;
   }

   double SMALL = 1e-10;   // Value used for coordinate equality check
   int nodeCount = NCN;    // This will be incremented as new mid-edge and mid-face nodes are added.
   int matchFound, ne, n1, n2, n3, n4, n5, n6, n7, n8;
   
   double midEdgeCoord[3];
   double midFaceCoord[3];
   double midElemCoord[3];


   int NneighborMEN;  // Number of mid-edge nodes of neighbors of an element
   int *neighborMEN;  // List of mid-edge nodes of neighbors of an element
   neighborMEN = new int [26*NEE];   // Maximum number of mid-edge nodes of neighbors of an element is NEF*NEE                                       // TODO: 26 is valid for a hexahedral element

   //Go through all edges of the element and check whether there are any new mid-edge nodes or not.
   for (int e = 0; e < NE; e++) {
   
      // Determine all mid-edge nodes of neighboring elements
      NneighborMEN = 0;           // Initialize
      for (int i = 0; i < 26*NEE; i++) {
         neighborMEN[i] = -1;     // Initialize
      }
      for (int e2 = 0; e2 < NelemNeighbors[e]; e2++) {  // Loop over element e's neighbors
         ne = elemNeighbors[e][e2];
         for (int i = NEC; i < NEC + NEE; i++) {
            if (LtoGnode[ne][i] != -1) {   // Check whether there is a previously found neighboring mid-edge node
               neighborMEN[NneighborMEN] = LtoGnode[ne][i];
               NneighborMEN++;
            }
         }
      }

      // CONTROL
      //cout << "NneighborMEN = " << NneighborMEN << endl;
      //cout << "e = " << e << ":  ";
      //for (int i = 0; i < NneighborMEN; i++) {
      //   cout << neighborMEN[i] << ",  ";
      //}
      //cout << "\n\n\n";
      
      for (int ed = 0; ed < NEE; ed++) {
         // Determine corner nodes of edge ed
         if (eType == 1) {   // Hexahedral element
            switch (ed) {
            case 0:
              n1 = LtoGnode[e][0];
              n2 = LtoGnode[e][1];
              break;
            case 1:
              n1 = LtoGnode[e][1];
              n2 = LtoGnode[e][2];
              break;
            case 2:
              n1 = LtoGnode[e][2];
              n2 = LtoGnode[e][3];
              break;
            case 3:
              n1 = LtoGnode[e][3];
              n2 = LtoGnode[e][0];
              break;
            case 4:
              n1 = LtoGnode[e][0];
              n2 = LtoGnode[e][4];
              break;
            case 5:
              n1 = LtoGnode[e][1];
              n2 = LtoGnode[e][5];
              break;
            case 6:
              n1 = LtoGnode[e][2];
              n2 = LtoGnode[e][6];
              break;
            case 7:
              n1 = LtoGnode[e][3];
              n2 = LtoGnode[e][7];
              break;
            case 8:
              n1 = LtoGnode[e][4];
              n2 = LtoGnode[e][5];
              break;
            case 9:
              n1 = LtoGnode[e][5];
              n2 = LtoGnode[e][6];
              break;
            case 10:
              n1 = LtoGnode[e][6];
              n2 = LtoGnode[e][7];
              break;
            case 11:
              n1 = LtoGnode[e][7];
              n2 = LtoGnode[e][4];
              break;
            }
         } else if (eType == 2) {   // Tetrahedral element
           printf("\n\n\nERROR: Tetrahedral elements are not implemented in function setupMidFaceNodes() yet!!!\n\n\n");
         }

         midEdgeCoord[0] = 0.5 * (coord[n1][0] + coord[n2][0]);
         midEdgeCoord[1] = 0.5 * (coord[n1][1] + coord[n2][1]);
         midEdgeCoord[2] = 0.5 * (coord[n1][2] + coord[n2][2]);

         matchFound = 0;
         
         // Search if this new mid-edge node coordinate was already found previously.
         int midEdgeNode;
         for (int i = 0; i < NneighborMEN; i++) {
            midEdgeNode = neighborMEN[i];   // Neighboring mid-edge node
            if (abs(midEdgeCoord[0] - coord[midEdgeNode][0]) < SMALL) {
               if (abs(midEdgeCoord[1] - coord[midEdgeNode][1]) < SMALL) {
                  if (abs(midEdgeCoord[2] - coord[midEdgeNode][2]) < SMALL) {   // Match found, this is not a new node.
                     LtoGnode[e][ed+NEC] = midEdgeNode;
                     matchFound = 1;
                     break;
                  }
               }
            }
         }
         
         /*
         // Search if this new coordinate was already found previously.
         for (int i = NCN; i<nodeCount; i++) {
            if (abs(midEdgeCoord[0] - coord[i][0]) < SMALL) {
               if (abs(midEdgeCoord[1] - coord[i][1]) < SMALL) {
                  if (abs(midEdgeCoord[2] - coord[i][2]) < SMALL) {   // Match found, this is not a new node.
                     LtoGnode[e][ed+NEC] = i;
                     matchFound = 1;
                     break;
                  }
               }
            }
         }
         */

         if (matchFound == 0) {   // No match found, this is a new node.
            LtoGnode[e][ed+NEC] = nodeCount;
            coord[nodeCount][0] = midEdgeCoord[0];
            coord[nodeCount][1] = midEdgeCoord[1];
            coord[nodeCount][2] = midEdgeCoord[2];
            nodeCount = nodeCount + 1;
         }
      }  // End of ed (edge) loop
   }  // End of e (element) loop

   delete[] neighborMEN;

   int lastNode = nodeCount;

   int NneighborMFN;  // Number of mid-face nodes of neighbors of an element
   int *neighborMFN;  // List of mid-face nodes of neighbors of an element
   neighborMFN = new int [26*NEF];   // Maximum number of mid-face nodes of neighbors of an element is 26*NEF                                        // TODO: 26 is valid for a hexahedral element

   // Go through all faces of the element and check whether there are any new mid-face nodes or not.
   for (int e = 0; e < NE; e++) {

      // Determine all mid-face nodes of neighboring elements
      NneighborMFN = 0;           // Initialize
      for (int i = 0; i < NEF*NEF; i++) {
         neighborMFN[i] = -1;     // Initialize
      }
      for (int e2 = 0; e2 < NelemNeighbors[e]; e2++) {  // Loop over element e's neighbors
         ne = elemNeighbors[e][e2];
         for (int i = NEC+NEE; i < NENv; i++) {
            if (LtoGnode[ne][i] != -1) {   // Check whether there is a previously found neighboring mid-face node
               neighborMFN[NneighborMFN] = LtoGnode[ne][i];
               NneighborMFN++;
            }
         }
      }

      /* CONTROL
      cout << "NneighborMFN = " << NneighborMFN << endl;
      cout << "e = " << e << ":  ";
      for (int i = 0; i < NneighborMFN; i++) {
         cout << neighborMFN[i] << ",  ";
      }
      cout << "\n\n\n";
      */

      for (int f = 0; f < NEF; f++) {
         // Determine corner nodes of face f
         if (eType == 1) {   // Hexahedral element
            switch (f) {
            case 0:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][1];
               n3 = LtoGnode[e][2];
               n4 = LtoGnode[e][3];
               break;
            case 1:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][1];
               n3 = LtoGnode[e][4];
               n4 = LtoGnode[e][5];
               break;
            case 2:
               n1 = LtoGnode[e][1];
               n2 = LtoGnode[e][2];
               n3 = LtoGnode[e][5];
               n4 = LtoGnode[e][6];
               break;
            case 3:
               n1 = LtoGnode[e][2];
               n2 = LtoGnode[e][3];
               n3 = LtoGnode[e][6];
               n4 = LtoGnode[e][7];
               break;
            case 4:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][3];
               n3 = LtoGnode[e][4];
               n4 = LtoGnode[e][7];
               break;
            case 5:
               n1 = LtoGnode[e][4];
               n2 = LtoGnode[e][5];
               n3 = LtoGnode[e][6];
               n4 = LtoGnode[e][7];
               break;
            }
           
            midFaceCoord[0] = 0.25 * (coord[n1][0] + coord[n2][0] + coord[n3][0] + coord[n4][0]);
            midFaceCoord[1] = 0.25 * (coord[n1][1] + coord[n2][1] + coord[n3][1] + coord[n4][1]);
            midFaceCoord[2] = 0.25 * (coord[n1][2] + coord[n2][2] + coord[n3][2] + coord[n4][2]);
         
         } else if (eType == 2) {   // Tetrahedral element
            printf("\n\n\nERROR: Tetrahedral elements are not implemented in function setupMidFaceNodes() yet!!!\n\n\n");
         }

         matchFound = 0;

         // Search if this new mid-face node coordinate was already found previously.
         int midFaceNode;
         for (int i = 0; i < NneighborMFN; i++) {
            midFaceNode = neighborMFN[i];   // Neighboring mid-face node
            if (abs(midFaceCoord[0] - coord[midFaceNode][0]) < SMALL) {
               if (abs(midFaceCoord[1] - coord[midFaceNode][1]) < SMALL) {
                  if (abs(midFaceCoord[2] - coord[midFaceNode][2]) < SMALL) {   // Match found, this is not a new node.
                     LtoGnode[e][f+NEC+NEE] = midFaceNode;
                     matchFound = 1;
                     break;
                  }
               }
            }
         }

         /*
         // Search if this new coordinate was already found previously.
         for (int i = lastNode; i<nodeCount; i++) {
            if (abs(midFaceCoord[0] - coord[i][0]) < SMALL) {
               if (abs(midFaceCoord[1] - coord[i][1]) < SMALL) {
                  if (abs(midFaceCoord[2] - coord[i][2]) < SMALL) {   // Match found, this is not a new node.
                     LtoGnode[e][f+NEC+NEE] = i;
                     matchFound = 1;
                     break;
                  }
               }
            }
         }
         */

         if (matchFound == 0) {   // No match found, this is a new node.
            LtoGnode[e][f+NEC+NEE] = nodeCount;
            coord[nodeCount][0] = midFaceCoord[0];
            coord[nodeCount][1] = midFaceCoord[1];
            coord[nodeCount][2] = midFaceCoord[2];
            nodeCount = nodeCount + 1;
         }
      }  // End of f (face) loop
   }  // End of e (element) loop

   delete[] neighborMFN;

   // Add the mid-element node as a new node.
   for (int e = 0; e < NE; e++) {
      if (eType == 1) {  // Hexahedral element
         n1 = LtoGnode[e][0];
         n2 = LtoGnode[e][1];
         n3 = LtoGnode[e][2];
         n4 = LtoGnode[e][3];
         n5 = LtoGnode[e][4];
         n6 = LtoGnode[e][5];
         n7 = LtoGnode[e][6];
         n8 = LtoGnode[e][7];
         
         midElemCoord[0] = 0.125 * (coord[n1][0] + coord[n2][0] + coord[n3][0] + coord[n4][0] + coord[n5][0] + coord[n6][0] + coord[n7][0] + coord[n8][0]);
         midElemCoord[1] = 0.125 * (coord[n1][1] + coord[n2][1] + coord[n3][1] + coord[n4][1] + coord[n5][1] + coord[n6][1] + coord[n7][1] + coord[n8][1]);
         midElemCoord[2] = 0.125 * (coord[n1][2] + coord[n2][2] + coord[n3][2] + coord[n4][2] + coord[n5][2] + coord[n6][2] + coord[n7][2] + coord[n8][2]);
         
         LtoGnode[e][NEC+NEE+NEF] = nodeCount;
         coord[nodeCount][0] = midElemCoord[0];
         coord[nodeCount][1] = midElemCoord[1];
         coord[nodeCount][2] = midElemCoord[2];
         nodeCount = nodeCount + 1;
      } else if (eType == 2) {   // Tetrahedral element
         printf("\n\n\nERROR: Tetrahedral elements are not implemented in function setupMidFaceNodes() yet!!!\n\n\n");
      }
  
   }  // End of e loop


   // From now on use NN instead of nodeCount
   NN = nodeCount;

   // CONTROL
   cout << "NN = " << NN << endl;
   
   //for (int e=0; e<NE; e++) {
   //   for(int i=0; i<NENv; i++) {
   //      cout << e << "  " << i << "  " << LtoGnode[e][i] << endl;
   //   } 
   //}




   // Decrease the size of coord by copying it and reallocating it with the correct size.
   double **copyCoord;
   copyCoord = new double*[NN];
   for (int i=0; i<NN; i++) {
      copyCoord[i] = new double[3];
   }

   for (int i=0; i<NN; i++) {
      for (int j=0; j<3; j++) {
         copyCoord[i][j] = coord[i][j];
      }
   }

   for (int i = 0; i<NE*NENv; i++) {
      delete[] coord[i];
   }
   delete[] coord;


   // Reallocate coord with the correct size NN.
   coord = new double*[NN];
   for (int i=0; i<NN; i++) {
      coord[i] = new double[3];
   }

   // Copy coordCopy back to coord.
   for (int i=0; i<NN; i++) {
      for (int j=0; j<3; j++) {
         coord[i][j] = copyCoord[i][j];
      }
   }

   // Delete copyCoord
   for (int i = 0; i<NN; i++) {
      delete[] copyCoord[i];
   }
   delete[] copyCoord;


   delete[] NelemNeighbors;

   for (int i = 0; i<NE; i++) {
      delete[] elemNeighbors[i];
   }
   delete[] elemNeighbors;

}  // End of function setupNonCornerNodes()





//========================================================================
void setupLtoGdof()
//========================================================================
{
   // Sets up LtoGvel and LtoGpres for each element using LtoGnode. As an
   // example LtoGvel uses the following local unknown ordering for a
   // quadrilateral element with NENv = 27
   //
   // u0, u1, u2, ..., u25, u26, v0, v1, v2, ..., v25, v26, w0, w1, w2, ..., w25, w26

   LtoGvel = new int*[NE];
   for (int i=0; i<NE; i++) {
      LtoGvel[i] = new int[3*NENv];                                                                                                                  // TODO : Actually 3*NENv size is unnecessary. Just NENv is enough. In that case just LtoGnode is enough, there is not need for LtoGvel.
   }

   LtoGpres = new int*[NE];
   for (int i=0; i<NE; i++) {
      LtoGpres[i] = new int[NENp];
   }

   int velCounter, presCounter;

   for (int e = 0; e < NE; e++) {
      velCounter = 0;   // Velocity unknown counter
      presCounter = 0;  // Pressure unknown counter
  
      // u velocity unknowns
      for (int i = 0; i<NENv; i++) {
         LtoGvel[e][velCounter] = LtoGnode[e][i];
         velCounter = velCounter + 1;
      }
  
      // v velocity unknowns
      for (int i = 0; i<NENv; i++) {
         LtoGvel[e][velCounter] = NN + LtoGnode[e][i];
         velCounter = velCounter + 1;
      }

      // w velocity unknowns
      for (int i = 0; i<NENv; i++) {
         LtoGvel[e][velCounter] = 2*NN + LtoGnode[e][i];
         velCounter = velCounter + 1;
      }

      // pressure unknowns
      // Note that first pressure unknown is numbered as 0, but not 3*NN.
      for (int i = 0; i < NENp; i++) {
         LtoGpres[e][presCounter] = LtoGnode[e][i];                                                                                                  // TODO: Aren't LtoGpres and part of LtoGnode the same for NENp=8 hexa elements?
         presCounter = presCounter + 1;
      }
   }

   /*  CONTROL
   for (int e=0; e<NE; e++) {
      for(int i=0; i<3*NENv; i++) {
         cout << e << "  " << i << "  " << LtoGvel[e][i] << endl;
      }
      cout << endl;
      for(int i=0; i<NENp; i++) {
         cout << e << "  " << i << "  " << LtoGpres[e][i] << endl;
      }
      cout << endl;
   }
   */

}  // End of function setupLtoGdof()





//========================================================================
void determineVelBCnodes()
//========================================================================
{
   // Element faces where velocity BCs are specified were read from the input
   // file. Now let's determine the actual nodes where these BCs are specified.

   int e, f, n1, n2, n3, n4, n5, whichBC;

   double* velBCinfo;   // Dummy variable to store which velocity BC is specified at a node.

   velBCinfo = new double[NN];

   for (int i = 0; i < NN; i++) {
      velBCinfo[i] = -1;    // Initialize to -1.
   }

   for (int i = 0; i < BCnVelFaces; i++) {
      e       = BCvelFaces[i][0];   // Element where velocity BC is specified.
      f       = BCvelFaces[i][1];   // Face where velocity BC is specified.
      whichBC = BCvelFaces[i][2];   // Number of specified BC.
  
      // Consider corner nodes of the face
      if (eType == 1) {   // Hexahedral element
         switch (f) {
         case 0:
            n1 = LtoGnode[e][0];
            n2 = LtoGnode[e][1];
            n3 = LtoGnode[e][2];
            n4 = LtoGnode[e][3];
            break;  
         case 1:
            n1 = LtoGnode[e][0];
            n2 = LtoGnode[e][1];
            n3 = LtoGnode[e][4];
            n4 = LtoGnode[e][5];
            break;
         case 2:
            n1 = LtoGnode[e][1];
            n2 = LtoGnode[e][2];
            n3 = LtoGnode[e][5];
            n4 = LtoGnode[e][6];
            break;
         case 3:
            n1 = LtoGnode[e][2];
            n2 = LtoGnode[e][3];
            n3 = LtoGnode[e][6];
            n4 = LtoGnode[e][7];
            break;
         case 4:
            n1 = LtoGnode[e][0];
            n2 = LtoGnode[e][3];
            n3 = LtoGnode[e][4];
            n4 = LtoGnode[e][7];
            break;
         case 5:
            n1 = LtoGnode[e][4];
            n2 = LtoGnode[e][5];
            n3 = LtoGnode[e][6];
            n4 = LtoGnode[e][7];
            break;
         }

         velBCinfo[n1] = whichBC;
         velBCinfo[n2] = whichBC;
         velBCinfo[n3] = whichBC;
         velBCinfo[n4] = whichBC;

      } else if (eType == 2) {   // Tetrahedral element
        printf("\n\n\nERROR: Tetrahedral elements are not implemented in function determineVelBCnodes() yet!!!\n\n\n");
      }
  
  
      // Consider mid-edge and mid-face nodes if there are any.
      if (NENp != NENv) {
         if (eType == 1) {  // Hexahedral element
            switch (f) {
            case 0:
               n1 = LtoGnode[e][8];
               n2 = LtoGnode[e][9];
               n3 = LtoGnode[e][10];
               n4 = LtoGnode[e][11];
               n5 = LtoGnode[e][20];
               break;
            case 1:
               n1 = LtoGnode[e][8];
               n2 = LtoGnode[e][12];
               n3 = LtoGnode[e][13];
               n4 = LtoGnode[e][16];
               n5 = LtoGnode[e][21];
               break;
            case 2:
               n1 = LtoGnode[e][9];
               n2 = LtoGnode[e][13];
               n3 = LtoGnode[e][14];
               n4 = LtoGnode[e][17];
               n5 = LtoGnode[e][22];
               break;
            case 3:
               n1 = LtoGnode[e][10];
               n2 = LtoGnode[e][14];
               n3 = LtoGnode[e][15];
               n4 = LtoGnode[e][18];
               n5 = LtoGnode[e][23];
               break;
            case 4:
               n1 = LtoGnode[e][11];
               n2 = LtoGnode[e][12];
               n3 = LtoGnode[e][15];
               n4 = LtoGnode[e][19];
               n5 = LtoGnode[e][24];
               break;
            case 5:
               n1 = LtoGnode[e][16];
               n2 = LtoGnode[e][17];
               n3 = LtoGnode[e][18];
               n4 = LtoGnode[e][19];
               n5 = LtoGnode[e][25];
               break;
            }

            velBCinfo[n1] = whichBC;
            velBCinfo[n2] = whichBC;
            velBCinfo[n3] = whichBC;
            velBCinfo[n4] = whichBC;
            velBCinfo[n5] = whichBC;

         } else if (eType == 2) {   // Tetrahedral element
            printf("\n\n\nERROR: Tetrahedral elements are not implemented in function determineVelBCnodes() yet!!!\n\n\n");
         }
      }
   }  // End of BCvelFaces loop


   // Count the number of velocity BC nodes
   BCnVelNodes = 0;
   for (int i = 0; i < NN; i++) {
      if (velBCinfo[i] != -1) {
         BCnVelNodes = BCnVelNodes + 1;
      }
   }

   // Store velBCinfo variable as BCvelNodes
   BCvelNodes = new int*[BCnVelNodes];

   for (int i = 0; i < BCnVelNodes; i++) {
      BCvelNodes[i] = new int[2];
   }

   int counter = 0;
   for (int i = 0; i < NN; i++) {
      if (velBCinfo[i] != -1) {
         BCvelNodes[counter][0] = i;
         BCvelNodes[counter][1] = int(velBCinfo[i]);
         counter = counter + 1;
      }
   }

   delete[] velBCinfo;
   delete[] BCvelFaces;


   //  CONTROL
   //for (int i=0; i<BCnVelNodes; i++) {
   //   cout << i << "  " << "  " << BCvelNodes[i][0] << "  " << BCvelNodes[i][1] << endl;
   //}
    
}  // End of function determineVelBCnodes()





//========================================================================
void findElemsOfVelNodes()
//========================================================================
{
   // Determines elements connected to velocity nodes (elemsOfVelNodes).
   // It is necessary for sparse storage. It is stored in a matrix of size
   // NNx10, where 10 is a number, estimated to be larger than the maximum 
   // number of elements connected to a velocity node.

   // Also an array (NelemOfVelNodes) stores the actual number of elements
   // connected to each velocity node.

   int LARGE = 10;  // It is assumed that not more than 10 elements are
                    // connected to a velocity node.
                                                                                                                                                     // TODO: Define this somewhere else, which will be easy to notice.
                                                                                                                                                     // TODO: Make sure that this is not violated.
   int node;

   elemsOfVelNodes = new int*[NN];
   for (int i=0; i<NN; i++) {
      elemsOfVelNodes[i] = new int[LARGE];
   }

   NelemOfVelNodes  = new int[NN];

   for (int i = 0; i < NN; i++) {
      NelemOfVelNodes[i] = 0;       // Initialize to zero
   }

   // Form elemsOfVelNodes using LtoGvel of each element
   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENv; i++) {
         node = LtoGvel[e][i];
         elemsOfVelNodes[node][NelemOfVelNodes[node]] = e;
         NelemOfVelNodes[node] = NelemOfVelNodes[node] + 1;
      }
   }

   //  CONTROL
   /*
   for (int i=0; i<NN; i++) {
      cout << i << "  " << NelemOfVelNodes[i] << endl;
   }

   cout << endl;

   for (int i=0; i<NNp; i++) {
      cout << i << "  " << NelemOfPresNodes[i] << endl;
   }
   */

}  // End of function findElemsOfVelNodes()





//========================================================================
void findMonitorPoint()
//========================================================================
{
   // Find the point that is closest to the monitor point coordinates read
   // from the input file.

   double distance = 1e6;   // Initialize to a large value

   double dx, dy, dz;

   for (int i = 0; i < NCN; i++) {
      dx = coord[i][0] - monPointCoord[0];
      dy = coord[i][1] - monPointCoord[1];
      dz = coord[i][2] - monPointCoord[2];
      
      if (sqrt(dx*dx + dy*dy + dz*dz) < distance) {
         distance = sqrt(dx*dx + dy*dy + dz*dz);
         monPoint = i;
      }
   }

   //  CONTROL
   // cout << monPoint << endl;

}  // End of function findMonitorPoint()





//========================================================================
void setupSparseM()
//========================================================================
{
   // Sets up row and column arrays of the global mass matrix.

   // First work only with the upper left part of the matrix and then extend it
   // for the middle and lower right parts, which are identical to the upper left
   // part.

   // In each row, find the columns with nonzero entries.

   int colCount;
   int LARGE;   // Maximum number of elements connected to a velocity node.

   // Determine the maximum number of elements connected to a velocity node.
   LARGE = 0;   // Initialize to a low value
   for (int i = 0; i < NN; i++) {
      if (NelemOfVelNodes[i] > LARGE) {
         LARGE = NelemOfVelNodes[i];
      }
   }

   // CONTROL
   // cout << endl << "LARGE = " << LARGE << endl;

   int sparseM_NNZ_onePart = 0;  // Counts nonzero entries in only 1 sub-mass matrix.

   int *NNZcolInARow;      // Number of nonzero columns in each row. This is nothing but the list of nodes that are in communication with each node.
   int **NZcolsInARow;     // Nonzero columns in each row.

   NNZcolInARow = new int[NN];

   NZcolsInARow = new int*[NN];
   for (int i=0; i<NN; i++) {
      NZcolsInARow[i] = new int[LARGE*NENv];                                                                                                         // TODO: This array may take too much memory in 3D. Instead this information can be stored in a CSR type arrangement.
   }

   int *isColNZ;      // A flag array to store whether the column is zero or not.
                      // Stores similar information as NZcolsInARow, but makes counting nonzeros easier.
   isColNZ = new int[NN];

   for (int r = 0; r < NN; r++) {   // Loop over all rows                                                                                            // TODO : This loop takes too much time
      for (int i = 0; i < NN; i++) {
         isColNZ[i] = 0;
      }
      colCount = 0;
  
      for (int i = 0; i < NelemOfVelNodes[r]; i++) {   // NelemOfVelNodes[r] is the number of elements connected to node r
         int e = elemsOfVelNodes[r][i];    // This element contributes to row r.
         for (int j = 0; j < NENv; j++) {
            if (isColNZ[LtoGvel[e][j]] == 0) {   // 0 means this column had no previous non zero contribution.
               isColNZ[LtoGvel[e][j]] = 1;    // 1 means this column is non zero.
               NZcolsInARow[r][colCount] = LtoGvel[e][j];
               colCount = colCount + 1;
            }
         }
      }
  
      NNZcolInARow[r] = colCount;
      sparseM_NNZ_onePart = sparseM_NNZ_onePart + colCount;
   }

   delete[] isColNZ;

   // Entries in each row of NZcolsInARow are not sorted. Let's sort them out.
   vector<int> toBeSorted;
   int nnz;
   for (int r = 0; r < NN; r++) {
      nnz = NNZcolInARow[r];   // Number of nonzeros in row r
      toBeSorted.resize(nnz);  // Will store nonzeros of row r for sorting.
      for (int i = 0; i < nnz; i++) {
         toBeSorted[i] = NZcolsInARow[r][i];   // Nonzero columns of row r (unsorted)
      }
      std::sort(toBeSorted.begin(), toBeSorted.end());     // Sorted version.
      
      // Copy sorted array back to NZcolsInARow
      for (int i = 0; i < nnz; i++) {
         NZcolsInARow[r][i] = toBeSorted[i];
      }
      
      toBeSorted.clear();  // Delete the entries in toBeSorted
   }

   /* CONTROL
   for (int i = 0; i < NN; i++) {
      for (int j = 0; j < NNZcolInARow[i]; j++) {
         cout << NZcolsInARow[i][j] << "  ";
      }
      cout << endl;
   }
   */

   waitForUser("OK1. Enter a character... ");

   // Allocate memory for 3 vectors of sparseM. Thinking about the whole
   // mass matrix, let's define the sizes properly by using three times of the
   // calculated NNZ.
   sparseMcol   = new int[3 * sparseM_NNZ_onePart];

   waitForUser("OK2. Enter a character... ");
   
   sparseMrow   = new int[3 * sparseM_NNZ_onePart];

   waitForUser("OK3. Enter a character... ");

   sparseMvalue = new double[3 * sparseM_NNZ_onePart];
   
   waitForUser("OK4. Enter a character... ");

   // Fill in soln.sparseM.col and soln.sparseM.row arrays
   // This is done in a row-by-row way.
   int NNZcounter = 0;
   for (int r = 0; r < NN; r++) {    // Loop over all rows
      for (int i = 0; i < NNZcolInARow[r]; i++) {
         sparseMrow[NNZcounter] = r;
         sparseMcol[NNZcounter] = NZcolsInARow[r][i];
         NNZcounter = NNZcounter + 1;
      }
   }

   // Actually mass matrix is not NNxNN, but 3NNx3NN. Its middle part and 
   // lower right parts are the same as its upper right part, and the other 6
   // blocks are full of zeros. What we set up above is only the upper right
   // part. Let's extend the row, col and value arrays to accomodate the
   // middle and lower right parts too.
                                                                                                                                                     // TODO: Is this expansion really necessary or can we save some memory by not doing it.
   for (int i = 0; i < sparseM_NNZ_onePart; i++) {
      sparseMrow[i +     sparseM_NNZ_onePart] = sparseMrow[i] + NN;
      sparseMrow[i + 2 * sparseM_NNZ_onePart] = sparseMrow[i] + 2*NN;
      sparseMcol[i +     sparseM_NNZ_onePart] = sparseMcol[i] + NN;
      sparseMcol[i + 2 * sparseM_NNZ_onePart] = sparseMcol[i] + 2*NN;
   }

   sparseM_NNZ = 3 * sparseM_NNZ_onePart;   // Triple the number of nonzeros.
   
   // CONTROL
   cout << endl << "sparseM_NNZ = " << sparseM_NNZ << endl;


   // Sparse storage of the K and A matrices are the same as M. Only extra
   // value arrays are necessary.
   sparseKvalue = new double[sparseM_NNZ];

   waitForUser("OK5. Enter a character... ");

   sparseAvalue = new double[sparseM_NNZ];

   waitForUser("OK6. Enter a character... ");

   // For CSR storage necessary for MKL and CUSP, we need rowIndex array too.
   sparseMrowIndex = new int[3*NN+1];
   sparseMrowIndex[0] = 0;
   sparseMrowIndex[3*NN+1] = sparseM_NNZ;
   for (int i = 1; i <= NN; i++) {
      sparseMrowIndex[i] = sparseMrowIndex[i-1] + NNZcolInARow[i-1];
   }

   for (int i = 1; i <= NN; i++) {
      sparseMrowIndex[i + NN] = sparseMrowIndex[i-1 + NN] + NNZcolInARow[i-1];
   }
   
   for (int i = 1; i <= NN; i++) {
      sparseMrowIndex[i + 2*NN] = sparseMrowIndex[i-1 + 2*NN] + NNZcolInARow[i-1];
   }

   // CONTROL
   //cout << sparseMrowIndex[3*NN+1]  << "   "  << sparseM_NNZ << endl;
   //for (int i = 0; i < 3*NN+1; i++) {
   //   cout << sparseMrowIndex[i] << endl;
   //}



   // Determine local-to-sparse mapping, i.e. find the location of the entries
   // of elemental sub-mass matrices in sparse storage. This will be used in
   // the assembly process.

   // First determine the nonzero entry number at the beginning of each row.
   // rowStarts[NN] is equal to NNZ+1.                                                                                                               // TODO: Is NNZ+1 correct for C++?
   int *rowStarts;
   rowStarts = new int[NN+1];

   waitForUser("OK7. Enter a character... ");

   rowStarts[0] = 0;   // First row starts with the zeroth entry
   for (int i = 1; i < NN+1; i++) {
      rowStarts[i] = rowStarts[i-1] + NNZcolInARow[i-1];
   }

   // CONTROL
   //for (int i = 0; i < NN+1; i++) {
   //   cout << rowStarts[i] << endl;
   //}

   sparseMapM = new int **[NE];
   for (int i = 0; i < NE; i++) {
      sparseMapM[i] = new int *[NENv];
      for (int j = 0; j < NENv; j++) {
         sparseMapM[i][j] = new int[NENv];
      }
   }

   waitForUser("OK8. Enter a character... ");

   int r, c;

   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENv; i++) {
         r = LtoGvel[e][i];

         int *col;    // Nonzeros of row r
         col = new int[rowStarts[r+1] - rowStarts[r]];
         
         for (int j = 0; j < NNZcolInARow[r]; j++) {
            col[j] = sparseMcol[j + rowStarts[r]];
         }

         for (int j = 0; j < NENv; j++) {
            // Find the location in the col array
            int jj = LtoGvel[e][j];
            for (c = 0; c < NNZcolInARow[r]; c++) {
               if (jj == col[c]) {
                  break;
               }
            }
            sparseMapM[e][i][j] = c + rowStarts[r];
         }
         delete[] col;
      }
   }

   /* CONTROL
   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENv; i++) {
         for (int j = 0; j < NENv; j++) {
            cout << sparseMapM[e][i][j] << endl;
         }
      }
      cout << endl;
   }
   */

   for (int i = 0; i<NN; i++) {
      delete[] NZcolsInARow[i];
   }
   delete[] NZcolsInARow;

   delete[] NNZcolInARow;
   delete[] rowStarts;

}  // End of function setupSparseM()






//========================================================================
void upperSparseM()
//========================================================================
{
   // Actually mass matrix is symmetric. We only need to store its upper
   // triangular part.




}  // End of function upperSparseM()





//========================================================================
void setupSparseG()
//========================================================================
{
   // Sets up row and column arrays of the global G matrix.

   // First work only with the upper part of the matrix and then extend it
   // for the lower part, which is identical to the upper part.


   // In each row, find the columns with nonzero entries.

   int colCount;
   int LARGE;   // Maximum number of elements connected to a pressure node.

   // Determine the maximum number of elements connected to a pressure node.
   LARGE = 0;   // Initialize to a low value
   for (int i = 0; i < NNp; i++) {
      if (NelemOfPresNodes[i] > LARGE) {
         LARGE = NelemOfPresNodes[i];
      }
   }

   // CONTROL
   // cout << endl << "LARGE = " << LARGE << endl;

   int sparseG_NNZ_onePart = 0;  // Counts nonzero entries in sub-G matrix.

   int *NNZcolInARow;      // Number of nonzero columns in each row. This is nothing but the list of nodes that are in communication with each node.
   int **NZcolsInARow;     // Nonzero columns in each row.

   NNZcolInARow = new int[NN];

   NZcolsInARow = new int*[NN];
   for (int i=0; i<NN; i++) {
      NZcolsInARow[i] = new int[LARGE*NENp];                                                                                                         // TODO: This array may take too much memory in 3D. Instead this information can be stored in a CSR type arrangement.
   }

   int *isColNZ;      // A flag array to store whether the column is zero or not.
                      // Stores similar information as NZcolsInARow, but makes counting nonzeros easier.
   isColNZ = new int[NNp];

   for (int r = 0; r < NN; r++) {   // Loop over all rows
      for (int i = 0; i < NNp; i++) {
         isColNZ[i] = 0;
      }
      colCount = 0;
  
      for (int i = 0; i < NelemOfVelNodes[r]; i++) {   // NelemOfVelNodes[r] is the number of elements connected to node r
         int e = elemsOfVelNodes[r][i];    // This element contributes to row r.
         for (int j = 0; j < NENp; j++) {
            if (isColNZ[LtoGpres[e][j]] == 0) {   // 0 means this column had no previous non zero contribution.
               isColNZ[LtoGpres[e][j]] = 1;    // 1 means this column is non zero.
               NZcolsInARow[r][colCount] = LtoGpres[e][j];
               colCount = colCount + 1;
            }
         }
      }
  
      NNZcolInARow[r] = colCount;
      sparseG_NNZ_onePart = sparseG_NNZ_onePart + colCount;
   }

   delete[] isColNZ;

   // Entries in each row of NZcolsInARow are not sorted. Let's sort them out.
   vector<int> toBeSorted;
   int nnz;
   for (int r = 0; r < NN; r++) {
      nnz = NNZcolInARow[r];   // Number of nonzeros in row r
      toBeSorted.resize(nnz);  // Will store nonzeros of row r for sorting.
      for (int i = 0; i < nnz; i++) {
         toBeSorted[i] = NZcolsInARow[r][i];   // Nonzero columns of row r (unsorted)
      }
      std::sort(toBeSorted.begin(), toBeSorted.end());     // Sorted version.
      
      // Copy sorted array back to NZcolsInARow
      for (int i = 0; i < nnz; i++) {
         NZcolsInARow[r][i] = toBeSorted[i];
      }
      
      toBeSorted.clear();  // Delete the entries in toBeSorted
   }

   /* CONTROL
   for (int i = 0; i < NN; i++) {
      for (int j = 0; j < NNZcolInARow[i]; j++) {
         cout << NZcolsInARow[i][j] << "  ";
      }
      cout << endl;
   }
   */


   // Allocate memory for 3 vectors of sparseG. Thinking about the whole
   // G matrix, let's define the sizes properly by using three times of the
   // calculated NNZ.
   sparseGcol   = new int[3 * sparseG_NNZ_onePart];
   sparseGrow   = new int[3 * sparseG_NNZ_onePart];
   sparseGvalue = new double[3 * sparseG_NNZ_onePart];

   // Fill in soln.sparseG.col and soln.sparseG.row arrays
   // This is done in a row-by-row way.
   int NNZcounter = 0;
   for (int r = 0; r < NN; r++) {    // Loop over all rows
      for (int i = 0; i < NNZcolInARow[r]; i++) {
         sparseGrow[NNZcounter] = r;
         sparseGcol[NNZcounter] = NZcolsInARow[r][i];
         NNZcounter = NNZcounter + 1;
      }
   }


   // Actually G matrix is not NNxNNp, but 3NNxNNp. Its middle and lower
   // parts are the same as its upper part. What we set up above is only the
   // upper part. Let's extend the row, col and value arrays to accomodate
   // the middle and lower parts too.
                                                                                                                                                     // TODO: Is this expansion really necessary or can we save some memory by not doing it.
   for (int i = 0; i < sparseG_NNZ_onePart; i++) {
      sparseGrow[i +     sparseG_NNZ_onePart] = sparseGrow[i] + NN;
      sparseGrow[i + 2 * sparseG_NNZ_onePart] = sparseGrow[i] + 2*NN;
      sparseGcol[i +     sparseG_NNZ_onePart] = sparseGcol[i];
      sparseGcol[i + 2 * sparseG_NNZ_onePart] = sparseGcol[i];
   }

   sparseG_NNZ = 3 * sparseG_NNZ_onePart;   // Triple the number of nonzeros.
   
   // CONTROL
   cout << endl << "sparseG_NNZ = " << sparseG_NNZ << endl;





   // For CSR storage necessary for MKL and CUSP, we need rowIndex array too.
   sparseGrowIndex = new int[3*NN+1];
   sparseGrowIndex[0] = 0;
   sparseGrowIndex[3*NN+1] = sparseG_NNZ;
   for (int i = 1; i <= NN; i++) {
      sparseGrowIndex[i] = sparseGrowIndex[i-1] + NNZcolInARow[i-1];
   }

   for (int i = 1; i <= NN; i++) {
      sparseGrowIndex[i + NN] = sparseGrowIndex[i-1 + NN] + NNZcolInARow[i-1];
   }
   
   for (int i = 1; i <= NN; i++) {
      sparseGrowIndex[i + 2*NN] = sparseGrowIndex[i-1 + 2*NN] + NNZcolInARow[i-1];
   }

   //cout << sparseGrowIndex[3*NN+1]  << "   "  << sparseG_NNZ << endl;
   //for (int i = 0; i < 3*NN+1; i++) {
   //   cout << sparseGrowIndex[i] << endl;
   //}
   //cout << "\n\n\n\n";


   // Determine local-to-sparse mapping, i.e. find the location of the entries
   // of elemental sub-G matrices in sparse storage. This will be used in
   // the assembly process.

   // First determine the nonzero entry number at the beginning of each row.
   // rowStarts[NN] is equal to NNZ+1.                                                                                                               // TODO: Is NNZ+1 correct for C++?
   int *rowStarts;
   rowStarts = new int[NN+1];
   rowStarts[0] = 0;   // First row starts with the zeroth entry
   for (int i = 1; i < NN+1; i++) {
      rowStarts[i] = rowStarts[i-1] + NNZcolInARow[i-1];
   }


   sparseMapG = new int **[NE];
   for (int i = 0; i < NE; i++) {
      sparseMapG[i] = new int *[NENv];
      for (int j = 0; j < NENv; j++) {
         sparseMapG[i][j] = new int[NENp];
      }
   }


   int r, c;

   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENv; i++) {
         r = LtoGvel[e][i];

         int *col;    // Nonzeros of row r
         col = new int[rowStarts[r+1] - rowStarts[r]];
         
         for (int j = 0; j < NNZcolInARow[r]; j++) {
            col[j] = sparseGcol[j + rowStarts[r]];
         }

         for (int j = 0; j < NENp; j++) {
            // Find the location in the col array
            int jj = LtoGpres[e][j];
            for (c = 0; c < NNZcolInARow[r]; c++){
               if (jj == col[c]) {
                  break;
               }
            }
            sparseMapG[e][i][j] = c + rowStarts[r];
          }
         delete[] col;
       }
   }

   /* CONTROL
   for (int e = 0; e < NE; e++) {
      for (int i = 0; i < NENv; i++) {
         for (int j = 0; j < NENp; j++) {
            cout << sparseMapG[e][i][j] << endl;
         }
      }
      cout << endl;
   }
   */


   for (int i = 0; i<NN; i++) {
      delete[] NZcolsInARow[i];
   }
   delete[] NZcolsInARow;

   delete[] NNZcolInARow;

   delete[] rowStarts;


   for (int i = 0; i<NN; i++) {
      delete[] elemsOfVelNodes[i];
   }
   delete[] elemsOfVelNodes;


   for (int i = 0; i<NNp; i++) {
      delete[] elemsOfPresNodes[i];
   }
   delete[] elemsOfPresNodes;


   delete[] NelemOfVelNodes;
   delete[] NelemOfPresNodes;

}  // End of function setupSparseG()





//========================================================================
void setupGQ()
//========================================================================
{
   GQpoint = new double*[NGP];
   for (int i=0; i<NGP; i++) {
      GQpoint[i] = new double[3];
   }

   GQweight = new double[NGP];

   
   if (eType == 1) {         // Hexahedral element
      if (NGP == 1)  {          // 1 point quadrature
         GQpoint[0][0] = 0.0;  GQpoint[0][1] = 0.0;  GQpoint[0][2] = 0.0;
         GQweight[0] = 4.0;                                                                                                                          // TODO: Is this correct?
      } else if (NGP == 8)  {   // 8 point quadrature
       GQpoint[0][0] = -sqrt(1./3);   GQpoint[0][1] = -sqrt(1./3);   GQpoint[0][2] = -sqrt(1./3);
       GQpoint[1][0] = sqrt(1./3);    GQpoint[1][1] = -sqrt(1./3);   GQpoint[1][2] = -sqrt(1./3);
       GQpoint[2][0] = -sqrt(1./3);   GQpoint[2][1] = sqrt(1./3);    GQpoint[2][2] = -sqrt(1./3);
       GQpoint[3][0] = sqrt(1./3);    GQpoint[3][1] = sqrt(1./3);    GQpoint[3][2] = -sqrt(1./3);
       GQpoint[4][0] = -sqrt(1./3);   GQpoint[4][1] = -sqrt(1./3);   GQpoint[4][2] = sqrt(1./3);
       GQpoint[5][0] = sqrt(1./3);    GQpoint[5][1] = -sqrt(1./3);   GQpoint[5][2] = sqrt(1./3);
       GQpoint[6][0] = -sqrt(1./3);   GQpoint[6][1] = sqrt(1./3);    GQpoint[6][2] = sqrt(1./3);
       GQpoint[7][0] = sqrt(1./3);    GQpoint[7][1] = sqrt(1./3);    GQpoint[7][2] = sqrt(1./3);
       GQweight[0] = 1.0;
       GQweight[1] = 1.0;
       GQweight[2] = 1.0;
       GQweight[3] = 1.0;
       GQweight[4] = 1.0;
       GQweight[5] = 1.0;
       GQweight[6] = 1.0;
       GQweight[7] = 1.0;
     } else if (NGP == 27) {    // 27 point quadrature
       
       // TODO : ...
       
     }
     
   } else if (eType == 2) {  // Tetrahedral element  
     
     // TODO : ...
     
   }
}  // End of function setupGQ()





//========================================================================
void calcShape()
//========================================================================
{
   // Calculates the values of the shape functions and their derivatives with
   // respect to ksi and eta at GQ points.

   // Sv, Sp   : Shape functions for velocity and pressure approximation.
   // dSv, dSp : ksi and eta derivatives of Sv and Sp.

   double ksi, eta, zeta;

   Sv = new double*[NENv];
   for (int i=0; i<NENv; i++) {
      Sv[i] = new double[NGP];
   }

   Sp = new double*[NENp];
   for (int i=0; i<NENp; i++) {
      Sp[i] = new double[NGP];
   }

   dSv = new double**[3];
   for (int i=0; i<3; i++) {
      dSv[i] = new double*[NENv];
      for (int j=0; j<NENv; j++) {
         dSv[i][j] = new double[NGP];
      }
   }

   dSp = new double**[3];
   for (int i=0; i<3; i++) {
      dSp[i] = new double*[NENp];
      for (int j=0; j<NENp; j++) {
         dSp[i][j] = new double[NGP];
      }
   }

   if (eType == 1) {  // Hexahedral element
     
      if (NENp == 8) {
         for (int k = 0; k < NGP; k++) {
            ksi  = GQpoint[k][0];
            eta  = GQpoint[k][1];
            zeta = GQpoint[k][2];
         
            Sp[0][k] = 0.125*(1-ksi)*(1-eta)*(1-zeta);
            Sp[1][k] = 0.125*(1+ksi)*(1-eta)*(1-zeta);
            Sp[2][k] = 0.125*(1+ksi)*(1+eta)*(1-zeta);
            Sp[3][k] = 0.125*(1-ksi)*(1+eta)*(1-zeta);
            Sp[4][k] = 0.125*(1-ksi)*(1-eta)*(1+zeta);
            Sp[5][k] = 0.125*(1+ksi)*(1-eta)*(1+zeta);
            Sp[6][k] = 0.125*(1+ksi)*(1+eta)*(1+zeta);
            Sp[7][k] = 0.125*(1-ksi)*(1+eta)*(1+zeta);
            
            // ksi derivatives of Sp
            dSp[0][0][k] = -0.125*(1-eta)*(1-zeta);
            dSp[0][1][k] =  0.125*(1-eta)*(1-zeta);
            dSp[0][2][k] =  0.125*(1+eta)*(1-zeta);
            dSp[0][3][k] = -0.125*(1+eta)*(1-zeta);
            dSp[0][4][k] = -0.125*(1-eta)*(1+zeta);
            dSp[0][5][k] =  0.125*(1-eta)*(1+zeta);
            dSp[0][6][k] =  0.125*(1+eta)*(1+zeta);
            dSp[0][7][k] = -0.125*(1+eta)*(1+zeta);
            
            // eta derivatives of Sp
            dSp[1][0][k] = -0.125*(1-ksi)*(1-zeta);
            dSp[1][1][k] = -0.125*(1+ksi)*(1-zeta);
            dSp[1][2][k] =  0.125*(1+ksi)*(1-zeta);
            dSp[1][3][k] =  0.125*(1-ksi)*(1-zeta);
            dSp[1][4][k] = -0.125*(1-ksi)*(1+zeta);
            dSp[1][5][k] = -0.125*(1+ksi)*(1+zeta);
            dSp[1][6][k] =  0.125*(1+ksi)*(1+zeta);
            dSp[1][7][k] =  0.125*(1-ksi)*(1+zeta);
         
            // zeta derivatives of Sp
            dSp[2][0][k] = -0.125*(1-ksi)*(1-eta);
            dSp[2][1][k] = -0.125*(1+ksi)*(1-eta);
            dSp[2][2][k] = -0.125*(1+ksi)*(1+eta);
            dSp[2][3][k] = -0.125*(1-ksi)*(1+eta);
            dSp[2][4][k] =  0.125*(1-ksi)*(1-eta);
            dSp[2][5][k] =  0.125*(1+ksi)*(1-eta);
            dSp[2][6][k] =  0.125*(1+ksi)*(1+eta);
            dSp[2][7][k] =  0.125*(1-ksi)*(1+eta);
         }
      } else {
         printf("\n\n\n ERROR: Only NENp = 8 is supported for hexahedral elements.\n\n\n");
      }
     
      if (NENv == 8) {
         //Sv = Sp;
         //dSv = dSp;
      } else if (NENv == 27) {
         for (int k = 0; k < NGP; k++) {
            ksi  = GQpoint[k][0];
            eta  = GQpoint[k][1];
            zeta = GQpoint[k][2];
         
            Sv[0][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta - eta) * (zeta*zeta - zeta);
            Sv[1][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta - eta) * (zeta*zeta - zeta);
            Sv[2][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta + eta) * (zeta*zeta - zeta);
            Sv[3][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta + eta) * (zeta*zeta - zeta);
            Sv[4][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta - eta) * (zeta*zeta + zeta);
            Sv[5][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta - eta) * (zeta*zeta + zeta);
            Sv[6][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta + eta) * (zeta*zeta + zeta);
            Sv[7][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta + eta) * (zeta*zeta + zeta);
         
            Sv[8][k]  = 0.25 * (1 - ksi*ksi) * (eta*eta - eta) * (zeta*zeta - zeta);
            Sv[9][k]  = 0.25 * (ksi*ksi + ksi) * (1 - eta*eta) * (zeta*zeta - zeta);
            Sv[10][k] = 0.25 * (1 - ksi*ksi) * (eta*eta + eta) * (zeta*zeta - zeta);
            Sv[11][k] = 0.25 * (ksi*ksi - ksi) * (1 - eta*eta) * (zeta*zeta - zeta);
         
            Sv[12][k] = 0.25 * (ksi*ksi - ksi) * (eta*eta - eta) * (1 - zeta*zeta);
            Sv[13][k] = 0.25 * (ksi*ksi + ksi) * (eta*eta - eta) * (1 - zeta*zeta);
            Sv[14][k] = 0.25 * (ksi*ksi + ksi) * (eta*eta + eta) * (1 - zeta*zeta);
            Sv[15][k] = 0.25 * (ksi*ksi - ksi) * (eta*eta + eta) * (1 - zeta*zeta);
         
            Sv[16][k] = 0.25 * (1 - ksi*ksi) * (eta*eta - eta) * (zeta*zeta + zeta);
            Sv[17][k] = 0.25 * (ksi*ksi + ksi) * (1 - eta*eta) * (zeta*zeta + zeta);
            Sv[18][k] = 0.25 * (1 - ksi*ksi) * (eta*eta + eta) * (zeta*zeta + zeta);
            Sv[19][k] = 0.25 * (ksi*ksi - ksi) * (1 - eta*eta) * (zeta*zeta + zeta);
         
            Sv[20][k] = 0.5 * (1 - ksi*ksi) * (1 - eta*eta) * (zeta*zeta - zeta);
            Sv[21][k] = 0.5 * (1 - ksi*ksi) * (eta*eta - eta) * (1 - zeta*zeta);
            Sv[22][k] = 0.5 * (ksi*ksi + ksi) * (1 - eta*eta) * (1 - zeta*zeta);
            Sv[23][k] = 0.5 * (1 - ksi*ksi) * (eta*eta + eta) * (1 - zeta*zeta);
            Sv[24][k] = 0.5 * (ksi*ksi - ksi) * (1 - eta*eta) * (1 - zeta*zeta);
            Sv[25][k] = 0.5 * (1 - ksi*ksi) * (1 - eta*eta) * (zeta*zeta + zeta);

            Sv[26][k] = (1 - ksi*ksi) * (1 - eta*eta) * (1 - zeta*zeta);

            // ksi derivatives of Sv
            dSv[0][0][k] = 0.125 * (2*ksi - 1) * (eta*eta - eta) * (zeta*zeta - zeta);
            dSv[0][1][k] = 0.125 * (2*ksi + 1) * (eta*eta - eta) * (zeta*zeta - zeta);
            dSv[0][2][k] = 0.125 * (2*ksi + 1) * (eta*eta + eta) * (zeta*zeta - zeta);
            dSv[0][3][k] = 0.125 * (2*ksi - 1) * (eta*eta + eta) * (zeta*zeta - zeta);
            dSv[0][4][k] = 0.125 * (2*ksi - 1) * (eta*eta - eta) * (zeta*zeta + zeta);
            dSv[0][5][k] = 0.125 * (2*ksi + 1) * (eta*eta - eta) * (zeta*zeta + zeta);
            dSv[0][6][k] = 0.125 * (2*ksi + 1) * (eta*eta + eta) * (zeta*zeta + zeta);
            dSv[0][7][k] = 0.125 * (2*ksi - 1) * (eta*eta + eta) * (zeta*zeta + zeta);
         
            dSv[0][8][k]  = 0.25 * (- 2*ksi) * (eta*eta - eta) * (zeta*zeta - zeta);
            dSv[0][9][k]  = 0.25 * (2*ksi + 1) * (1 - eta*eta) * (zeta*zeta - zeta);
            dSv[0][10][k] = 0.25 * (- 2*ksi) * (eta*eta + eta) * (zeta*zeta - zeta);
            dSv[0][11][k] = 0.25 * (2*ksi - 1) * (1 - eta*eta) * (zeta*zeta - zeta);
         
            dSv[0][12][k] = 0.25 * (2*ksi - 1) * (eta*eta - eta) * (1 - zeta*zeta);
            dSv[0][13][k] = 0.25 * (2*ksi + 1) * (eta*eta - eta) * (1 - zeta*zeta);
            dSv[0][14][k] = 0.25 * (2*ksi + 1) * (eta*eta + eta) * (1 - zeta*zeta);
            dSv[0][15][k] = 0.25 * (2*ksi - 1) * (eta*eta + eta) * (1 - zeta*zeta);
         
            dSv[0][16][k] = 0.25 * (- 2*ksi) * (eta*eta - eta) * (zeta*zeta + zeta);
            dSv[0][17][k] = 0.25 * (2*ksi + 1) * (1 - eta*eta) * (zeta*zeta + zeta);
            dSv[0][18][k] = 0.25 * (- 2*ksi) * (eta*eta + eta) * (zeta*zeta + zeta);
            dSv[0][19][k] = 0.25 * (2*ksi - 1) * (1 - eta*eta) * (zeta*zeta + zeta);
         
            dSv[0][20][k] = 0.5 * (- 2*ksi) * (1 - eta*eta) * (zeta*zeta - zeta);
            dSv[0][21][k] = 0.5 * (- 2*ksi) * (eta*eta - eta) * (1 - zeta*zeta);
            dSv[0][22][k] = 0.5 * (2*ksi + 1) * (1 - eta*eta) * (1 - zeta*zeta);
            dSv[0][23][k] = 0.5 * (- 2*ksi) * (eta*eta + eta) * (1 - zeta*zeta);
            dSv[0][24][k] = 0.5 * (2*ksi - 1) * (1 - eta*eta) * (1 - zeta*zeta);
            dSv[0][25][k] = 0.5 * (- 2*ksi) * (1 - eta*eta) * (zeta*zeta + zeta);

            dSv[0][26][k] = (- 2*ksi) * (1 - eta*eta) * (1 - zeta*zeta);
         
         
            // eta derivatives of Sv
            dSv[1][0][k] = 0.125 * (ksi*ksi - ksi) * (2*eta - 1) * (zeta*zeta - zeta);
            dSv[1][1][k] = 0.125 * (ksi*ksi + ksi) * (2*eta - 1) * (zeta*zeta - zeta);
            dSv[1][2][k] = 0.125 * (ksi*ksi + ksi) * (2*eta + 1) * (zeta*zeta - zeta);
            dSv[1][3][k] = 0.125 * (ksi*ksi - ksi) * (2*eta + 1) * (zeta*zeta - zeta);
            dSv[1][4][k] = 0.125 * (ksi*ksi - ksi) * (2*eta - 1) * (zeta*zeta + zeta);  
            dSv[1][5][k] = 0.125 * (ksi*ksi + ksi) * (2*eta - 1) * (zeta*zeta + zeta);
            dSv[1][6][k] = 0.125 * (ksi*ksi + ksi) * (2*eta + 1) * (zeta*zeta + zeta);
            dSv[1][7][k] = 0.125 * (ksi*ksi - ksi) * (2*eta + 1) * (zeta*zeta + zeta);
         
            dSv[1][8][k]  = 0.25 * (1 - ksi*ksi) * (2*eta - 1) * (zeta*zeta - zeta);
            dSv[1][9][k]  = 0.25 * (ksi*ksi + ksi) * (- 2*eta) * (zeta*zeta - zeta);
            dSv[1][10][k] = 0.25 * (1 - ksi*ksi) * (2*eta + 1) * (zeta*zeta - zeta);
            dSv[1][11][k] = 0.25 * (ksi*ksi - ksi) * (- 2*eta) * (zeta*zeta - zeta);
         
            dSv[1][12][k] = 0.25 * (ksi*ksi - ksi) * (2*eta - 1) * (1 - zeta*zeta);
            dSv[1][13][k] = 0.25 * (ksi*ksi + ksi) * (2*eta - 1) * (1 - zeta*zeta);
            dSv[1][14][k] = 0.25 * (ksi*ksi + ksi) * (2*eta + 1) * (1 - zeta*zeta);
            dSv[1][15][k] = 0.25 * (ksi*ksi - ksi) * (2*eta + 1) * (1 - zeta*zeta);
         
            dSv[1][16][k] = 0.25 * (1 - ksi*ksi) * (2*eta - 1) * (zeta*zeta + zeta);
            dSv[1][17][k] = 0.25 * (ksi*ksi + ksi) * (- 2*eta) * (zeta*zeta + zeta);
            dSv[1][18][k] = 0.25 * (1 - ksi*ksi) * (2*eta + 1) * (zeta*zeta + zeta);
            dSv[1][19][k] = 0.25 * (ksi*ksi - ksi) * (- 2*eta) * (zeta*zeta + zeta);
         
            dSv[1][20][k] = 0.5 * (1 - ksi*ksi) * (- 2*eta) * (zeta*zeta - zeta);
            dSv[1][21][k] = 0.5 * (1 - ksi*ksi) * (2*eta - 1) * (1 - zeta*zeta);
            dSv[1][22][k] = 0.5 * (ksi*ksi + ksi) * (- 2*eta) * (1 - zeta*zeta);
            dSv[1][23][k] = 0.5 * (1 - ksi*ksi) * (2*eta + 1) * (1 - zeta*zeta);
            dSv[1][24][k] = 0.5 * (ksi*ksi - ksi) * (- 2*eta) * (1 - zeta*zeta);
            dSv[1][25][k] = 0.5 * (1 - ksi*ksi) * (- 2*eta) * (zeta*zeta + zeta);

            dSv[1][26][k] = (1 - ksi*ksi) * (- 2*eta) * (1 - zeta*zeta);
        
         
            // zeta derivatives of Sv
            dSv[2][0][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta - eta) * (2*zeta - 1);
            dSv[2][1][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta - eta) * (2*zeta - 1);
            dSv[2][2][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta + eta) * (2*zeta - 1);
            dSv[2][3][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta + eta) * (2*zeta - 1);
            dSv[2][4][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta - eta) * (2*zeta + 1);
            dSv[2][5][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta - eta) * (2*zeta + 1);
            dSv[2][6][k] = 0.125 * (ksi*ksi + ksi) * (eta*eta + eta) * (2*zeta + 1);
            dSv[2][7][k] = 0.125 * (ksi*ksi - ksi) * (eta*eta + eta) * (2*zeta + 1);
         
            dSv[2][8][k]  = 0.25 * (1 - ksi*ksi) * (eta*eta - eta) * (2*zeta - 1);
            dSv[2][9][k]  = 0.25 * (ksi*ksi + ksi) * (1 - eta*eta) * (2*zeta - 1);
            dSv[2][10][k] = 0.25 * (1 - ksi*ksi) * (eta*eta + eta) * (2*zeta - 1);
            dSv[2][11][k] = 0.25 * (ksi*ksi - ksi) * (1 - eta*eta) * (2*zeta - 1);
         
            dSv[2][12][k] = 0.25 * (ksi*ksi - ksi) * (eta*eta - eta) * (- 2*zeta);
            dSv[2][13][k] = 0.25 * (ksi*ksi + ksi) * (eta*eta - eta) * (- 2*zeta);
            dSv[2][14][k] = 0.25 * (ksi*ksi + ksi) * (eta*eta + eta) * (- 2*zeta);
            dSv[2][15][k] = 0.25 * (ksi*ksi - ksi) * (eta*eta + eta) * (- 2*zeta);
         
            dSv[2][16][k] = 0.25 * (1 - ksi*ksi) * (eta*eta - eta) * (2*zeta + 1);
            dSv[2][17][k] = 0.25 * (ksi*ksi + ksi) * (1 - eta*eta) * (2*zeta + 1);
            dSv[2][18][k] = 0.25 * (1 - ksi*ksi) * (eta*eta + eta) * (2*zeta + 1);
            dSv[2][19][k] = 0.25 * (ksi*ksi - ksi) * (1 - eta*eta) * (2*zeta + 1);
         
            dSv[2][20][k] = 0.5 * (1 - ksi*ksi) * (1 - eta*eta) * (2*zeta - 1);
            dSv[2][21][k] = 0.5 * (1 - ksi*ksi) * (eta*eta - eta) * (- 2*zeta);
            dSv[2][22][k] = 0.5 * (ksi*ksi + ksi) * (1 - eta*eta) * (- 2*zeta);
            dSv[2][23][k] = 0.5 * (1 - ksi*ksi) * (eta*eta + eta) * (- 2*zeta);
            dSv[2][24][k] = 0.5 * (ksi*ksi - ksi) * (1 - eta*eta) * (- 2*zeta);
            dSv[2][25][k] = 0.5 * (1 - ksi*ksi) * (1 - eta*eta) * (2*zeta + 1);

            dSv[2][26][k] = (1 - ksi*ksi) * (1 - eta*eta) * (- 2*zeta);
         }
      } else {
         printf("\n\n\n ERROR: Only NENv = 8 and 27 are supported for hexahedral elements.\n\n\n");
      }
     
   } else if (eType == 2) { // Tetrahedral element
     
      // TODO : ...
   
   }  // End of eType

   /* CONTROL
   for (int i = 0; i < 3; i++){
     for (int j = 0; j < NENp; j++) {
         for (int k = 0; k < NGP; k++) {
            cout << i << "  " << j << "  "  << k << "  " << dSp[i][j][k] << endl;
         }
      }
   }
   */

}  // End of function calcShape()





//------------------------------------------------------------------------------
void calcJacob()
//------------------------------------------------------------------------------
{
   // Calculates Jacobian matrix and its determinant for each element. Shape
   // functions for corner nodes (pressure nodes) are used for Jacobian
   // calculation.
   // Also calculates and stores the derivatives of velocity shape functions
   // wrt x and y at GQ points for each element.

   int iG; 
   double **e_coord;
   double **Jacob, **invJacob;
   
   e_coord = new double*[NEC];
   
   for (int i = 0; i < NEC; i++) {
      e_coord[i] = new double[3];
   }

   Jacob = new double*[3];
   invJacob = new double*[3];
   for (int i = 0; i < 3; i++) {
      Jacob[i] = new double[3];
      invJacob[i] = new double[3];
   }
   
   detJacob = new double*[NE];
   for (int i = 0; i < NE; i++) {
      detJacob[i] = new double[NGP];
   }
   
   gDSp = new double***[NE];

   for (int i = 0; i < NE; i++) {
      gDSp[i] = new double**[3];
      for(int j = 0; j < 3; j++) {
         gDSp[i][j] = new double*[NENp];     
         for(int k = 0; k < NENp; k++) {
            gDSp[i][j][k] = new double[NGP];
         }
      }	
   }
      
   gDSv = new double***[NE];

   for (int i = 0; i < NE; i++) {
      gDSv[i] = new double**[3];
      for(int j = 0; j < 3; j++) {
         gDSv[i][j] = new double*[NENv];
         for(int k = 0; k < NENv; k++) {
            gDSv[i][j][k] = new double[NGP];
         }
      }	
   }      

   for (int e = 0; e < NE; e++){
      // Find e_ccord, coordinates for NEC corners of element e.
      for (int i = 0; i < NEC; i++){
         iG = LtoGnode[e][i];
         e_coord[i][0] = coord[iG][0]; 
         e_coord[i][1] = coord[iG][1];
         e_coord[i][2] = coord[iG][2];
      }
   
      // For each GQ point calculate 3x3 Jacobian matrix, its inverse and its
      // determinant. Also calculate derivatives of shape functions wrt global
      // coordinates x, y & z. These are the derivatives that we'll use in
      // evaluating integrals of elemental systems.

      double sum;

      for (int k = 0; k < NGP; k++) {
         for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
               sum = 0;
               for (int m = 0; m < NENp; m++) {
                  sum = sum + dSp[i][m][k] * e_coord[m][j];
               }
               Jacob[i][j] = sum;
            }
         }
         
         invJacob[0][0] =   Jacob[1][1]*Jacob[2][2] - Jacob[2][1]*Jacob[1][2];
         invJacob[0][1] = -(Jacob[0][1]*Jacob[2][2] - Jacob[0][2]*Jacob[2][1]);
         invJacob[0][2] =   Jacob[0][1]*Jacob[1][2] - Jacob[1][1]*Jacob[0][2];
         invJacob[1][0] = -(Jacob[1][0]*Jacob[2][2] - Jacob[1][2]*Jacob[2][0]);
         invJacob[1][1] =   Jacob[2][2]*Jacob[0][0] - Jacob[2][0]*Jacob[0][2];
         invJacob[1][2] = -(Jacob[1][2]*Jacob[0][0] - Jacob[1][0]*Jacob[0][2]);
         invJacob[2][0] =   Jacob[1][0]*Jacob[2][1] - Jacob[2][0]*Jacob[1][1];
         invJacob[2][1] = -(Jacob[2][1]*Jacob[0][0] - Jacob[2][0]*Jacob[0][1]);
         invJacob[2][2] =   Jacob[1][1]*Jacob[0][0] - Jacob[1][0]*Jacob[0][1];

         detJacob[e][k] = Jacob[0][0]*(Jacob[1][1]*Jacob[2][2] - Jacob[2][1]*Jacob[1][2]) +
                          Jacob[0][1]*(Jacob[1][2]*Jacob[2][0] - Jacob[1][0]*Jacob[2][2]) +
                          Jacob[0][2]*(Jacob[1][0]*Jacob[2][1] - Jacob[1][1]*Jacob[2][0]);
         
         for (int i = 0; i < 3; i++){
            for (int j = 0; j < 3; j++){
               invJacob[i][j] = invJacob[i][j] / detJacob[e][k];
            }    
         }
         
         for (int i = 0; i < 3; i++){
            for (int j = 0; j < NENp; j++) {
               sum = 0;
               for (int m = 0; m < 3; m++) { 
                  sum = sum + invJacob[i][m] * dSp[m][j][k];
               }
               gDSp[e][i][j][k] = sum;
            }
         }

         for (int i = 0; i < 3; i++){
            for (int j = 0; j < NENv; j++) {
               sum = 0;
               for (int m = 0; m < 3; m++) { 
                  sum = sum + invJacob[i][m] * dSv[m][j][k];
               }
               gDSv[e][i][j][k] = sum;
            }
         }

      /* CONTROL
      cout << e << "  " << k << "  " << detJacob[e][k] << endl;

      for (int i = 0; i < 3; i++){
         for (int j = 0; j < 3; j++){
            cout << e << "  " << i << "  " << j << "  " << invJacob[i][j] << endl;
         }
      }
      */

      }   // End of GQ loop

   }   // End of element loop

   
   /*  CONTROL
   for (int e = 0; e < 1; e++){
      for (int i = 0; i < 3; i++){
         for (int j = 0; j < NENv; j++) {
            for (int k = 0; k < NGP; k++) {
               cout << e << "  " << i << "  " << j << "  "  << k << "  " << gDSv[e][i][j][k] << endl;
            }
         }
      }
   }
   */


   // Deallocate unnecessary variables

   for (int i = 0; i < 3; i++) {
      delete[] Jacob[i];
   }
   delete[] Jacob;

   for (int i = 0; i < 3; i++) {
      delete[] invJacob[i];
   }
   delete[] invJacob;
   
   for (int i = 0; i < NENp; i++) {
      delete[] e_coord[i];
   }
   delete[] e_coord;

                                                                                                                                                     // TODO : Deallocate dSv and dSv.
} // End of function calcJacob()





//========================================================================
void initializeAndAllocate()
//========================================================================
{
   // Do the necessary memory allocations. Apply the initial condition or read
   // the restart file.

   Un           = new double[3*NN];     // x, y and z velocity components of time step n.
   Unp1         = new double[3*NN];     // U_i+1^n+1 of the reference paper.
   Unp1_prev    = new double[3*NN];     // U_i^n+1 of the reference paper.
   UnpHalf      = new double[3*NN];     // U_i+1^n+1/2 of the reference paper.
   UnpHalf_prev = new double[3*NN];     // U_i^n+1/2 of the reference paper.

   AccHalf      = new double[3*NN];     // A_i+1^n+1/2 of the reference paper.
   Acc          = new double[3*NN];     // A_i+1^n+1 of the reference paper.
   Acc_prev     = new double[3*NN];     // A_i^n+1 of the reference paper.

   Pn        = new double[NNp];         // Pressure of time step n.
   Pnp1      = new double[NNp];         // U_i+1^n+1 of the reference paper.
   Pnp1_prev = new double[NNp];         // p_i+1^n+1 of the reference paper.
   Pdot      = new double[NNp];         // Pdot_i+1^n+1 of the reference paper.

   Md        = new double[3*NN];        // Diagonalized mass matrix with BCs applied
   MdOrig    = new double[3*NN];        // Diagonalized mass matrix without BCs applied
   MdInv     = new double[3*NN];        // Inverse of the diagonalized mass matrix with BCs applied
   MdOrigInv = new double[3*NN];        // Inverse of the diagonalized mass matrix without BCs applied

   sparseMdOrigInvTimesKvalue = new double[sparseM_NNZ];   // Values of inv(Md) * K sparse matrix

   R1 = new double[3*NN];               // RHS vector of intermediate velocity calculation.
   R2 = new double[NNp];                // RHS vector of pressure calculation.
   R3 = new double[3*NN];               // RHS vector of new velocity calculation.


   // Initialize all these variables to zero
   for (int i = 0; i < 3*NN; i++) {
      Un[i]              = 0.0;
      Unp1[i]            = 0.0;
      Unp1_prev[i]       = 0.0;
      UnpHalf[i]         = 0.0;
      UnpHalf_prev[i]    = 0.0;
      AccHalf[i]         = 0.0;
      Acc[i]             = 0.0;
      Acc_prev[i]        = 0.0;
      Md[i]              = 0.0;
      MdOrig[i]          = 0.0;
      MdOrigInv[i]       = 0.0;
      R1[i] = 0.0;
      R3[i] = 0.0;
   }

   for (int i = 0; i < NNp; i++) {
      Pn[i]        = 0.0;
      Pnp1[i]      = 0.0;
      Pnp1_prev[i] = 0.0;
      Pdot[i]      = 0.0;
      R2[i]        = 0.0;
   }


   // Read the restart file if isRestart is equal to 1. If not, apply the
   // specified BCs.
   if (isRestart == 1) {
     readRestartFile();
   } else {
     applyBC_initial();
   }

   createTecplot();

   // Initialize discrete time level and time.
   timeN = 0;
   timeT = t_ini;

}  // End of function initializeAndAllocate()





//========================================================================
void timeLoop()
//========================================================================
{
   // Main time loop of the solution.

   double Start, End;
   int iter;

   // Initialize the solution using the specified initial condition and do
   // memory allocations.
   initializeAndAllocate();


   // Calculate certain matrices and their inverses only once before the time loop.
   Start = getHighResolutionTime();
   step0();
   End = getHighResolutionTime();
   printf("step0()               took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

   waitForUser("Enter a character... ");

   printf("Monitoring node is %d, with coordinates [%f, %f, %f].\n\n",
           monPoint, coord[monPoint][0], coord[monPoint][1], coord[monPoint][2]);

   printf("Time step  Iter     Time       u_monitor     v_monitor     w_monitor     p_monitor\n");
   printf("-------------------------------------------------------------------------------------\n");


   while (timeT < t_final) {  // Time loop
      timeN = timeN + 1;
      timeT = timeT + dt;
     
      // Initialize variables for the first iteration of the following loop.
      for (int i = 0; i < 3*NN; i++) {
         UnpHalf_prev[i] = Un[i];
         Unp1_prev[i] = Un[i];
         Acc_prev[i] = 0.0;
      }

      for (int i = 0; i < NNp; i++) {
         Pnp1_prev[i] = Pn[i];
      }


      // Iterations inside a time step
      for (iter = 1; iter <= maxIter; iter++) {
         // Calculate intermediate velocity.
         Start = getHighResolutionTime();
         step1(iter);
         End = getHighResolutionTime();
         printf("step1()               took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

         waitForUser("Enter a character... ");

         // Calculate pressure of the new time step
         Start = getHighResolutionTime();
         step2();
         End = getHighResolutionTime();
         printf("step2()               took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

         waitForUser("Enter a character... ");

         // Calculate velocity of the new time step
         Start = getHighResolutionTime();
         step3();
         End = getHighResolutionTime();
         printf("step3()               took  %8.3f seconds.\n", (End - Start) / CLOCKS_PER_SEC);

         waitForUser("Enter a character... ");
       
         // Check for convergence
         double sum1, sum2;
         sum1 = 0.0;
         sum2 = 0.0;
         for (int i=0; i<3*NN; i++) {
            sum1 = sum1 + Unp1[i] * Unp1[i];
            sum2 = sum2 + (Unp1[i] - Unp1_prev[i]) * (Unp1[i] - Unp1_prev[i]);
         }
         double normalizedNorm1 = sqrt(sum2) / sqrt(sum1);

         sum1 = 0.0;
         sum2 = 0.0;
         for (int i=0; i<NNp; i++) {
            sum1 = sum1 + Pnp1[i] * Pnp1[i];
            sum2 = sum2 + (Pnp1[i] - Pnp1_prev[i]) * (Pnp1[i] - Pnp1_prev[i]);
         }
         double normalizedNorm2 = sqrt(sum2) / sqrt(sum1);

         // CONTROL
         //cout << "timeT = " << timeT << ",   iter = " << iter << endl;
         //for(int i=0; i < 3*NN; i++) {
         //   cout << UnpHalf[i] << "   " << Unp1[i] << "   " << Acc[i] << endl;
         //}

         if (normalizedNorm1 < tolerance && normalizedNorm2 < tolerance) {
            break;
         }
       
         // Get ready for the next iteration
         for (int i = 0; i < 3*NN; i++) {
            UnpHalf_prev[i] = UnpHalf[i];
            Unp1_prev[i] = Unp1[i];
            Acc_prev[i] = Acc[i];
         }

         for (int i = 0; i < NNp; i++) {
            Pnp1_prev[i] = Pnp1[i];
         }
      }  // End of iter loop
     
     
      // Get ready for the next time step
      for (int i = 0; i < 3*NN; i++) {
         Un[i] = Unp1[i];
      }

      for (int i = 0; i < NNp; i++) {
         Pn[i] = Pnp1[i];
      }
     
     
      if (timeN % 50 == 0 || abs(timeT - t_final) < 1e-10) {
         createTecplot();
      }
     
     
      // Print monitor point data
      printf("\n%6d  %6d  %10.5f  %12.5f  %12.5f  %12.5f  %12.5f\n",
             timeN, iter, timeT, Un[monPoint],
             Un[NN+monPoint], Un[2*NN+monPoint], Pn[monPoint]);

   }  // End of while loop for time

}  // End of function timeLoop()





//========================================================================
void step0()
//========================================================================
{
   // Calculates [M], [G] and [K] only once, before the time loop.

   int nnzM = sparseM_NNZ / 3;
   int nnzM2 = 2 * nnzM;
   int nnzM3 = 3 * nnzM;

   int nnzG = sparseG_NNZ / 3;
   int nnzG2 = 2 * nnzG;

   double **Me_11, **Ke_11, **Ge_1, **Ge_2, **Ge_3;
   double GQfactor;

   for (int i = 0; i < sparseM_NNZ; i++){
      sparseMvalue[i] = 0.0;
	   sparseKvalue[i] = 0.0;
   }

   for (int i = 0; i < sparseG_NNZ; i++){
      sparseGvalue[i] = 0.0;
   }

   Me_11 = new double*[NENv];
   Ke_11 = new double*[NENv];
   for (int i = 0; i < NENv; i++) {
      Me_11[i] = new double[NENv];
      Ke_11[i] = new double[NENv];
   }

   Ge_1 = new double*[NENv];
   Ge_2 = new double*[NENv];
   Ge_3 = new double*[NENv];
   for (int i = 0; i < NENv; i++) {
      Ge_1[i] = new double[NENp];
      Ge_2[i] = new double[NENp];
      Ge_3[i] = new double[NENp];
   }

   // Calculate Me, Ke and Ge, and assemble them into M, K and G
   for (int e = 0; e < NE; e++) {

      for (int i = 0; i < NENv; i++) {
         for (int j = 0; j < NENv; j++) {
            Me_11[i][j] = 0.0;
            Ke_11[i][j] = 0.0;
         }
         for (int j = 0; j < NENp; j++) {
            Ge_1[i][j] = 0.0;
            Ge_2[i][j] = 0.0;
            Ge_3[i][j] = 0.0;
         }
      }

      for (int k = 0; k < NGP; k++) {   // Gauss Quadrature loop
         GQfactor = detJacob[e][k] * GQweight[k];
       
         for (int i = 0; i < NENv; i++) {
            for (int j = 0; j < NENv; j++) {
               Me_11[i][j] = Me_11[i][j] + Sv[i][k] * Sv[j][k] * GQfactor;
               Ke_11[i][j] = Ke_11[i][j] + viscosity * (gDSv[e][0][i][k] * gDSv[e][0][j][k] +
                                                        gDSv[e][1][i][k] * gDSv[e][1][j][k] +
                                                        gDSv[e][2][i][k] * gDSv[e][2][j][k]) * GQfactor;
            }
         }

         for (int i = 0; i < NENv; i++) {
            for (int j = 0; j < NENp; j++) {
               Ge_1[i][j] = Ge_1[i][j] - Sp[j][k] * gDSv[e][0][i][k] * GQfactor;
               Ge_2[i][j] = Ge_2[i][j] - Sp[j][k] * gDSv[e][1][i][k] * GQfactor;
               Ge_3[i][j] = Ge_3[i][j] - Sp[j][k] * gDSv[e][2][i][k] * GQfactor;
            }
         }
       
      } // GQ loop
     
     
      // Assemble Me and Ke into sparse M and K.
      for (int i = 0; i < NENv; i++) {
         for (int j = 0; j < NENv; j++) {
            sparseMvalue[sparseMapM[e][i][j]]         += Me_11[i][j];   // Assemble upper left sub-matrix of M
            sparseMvalue[sparseMapM[e][i][j] + nnzM]  += Me_11[i][j];   // Assemble middle sub-matrix of M
            sparseMvalue[sparseMapM[e][i][j] + nnzM2] += Me_11[i][j];   // Assemble lower right sub-matrix of M

            sparseKvalue[sparseMapM[e][i][j]]         += Ke_11[i][j];   // Assemble upper left sub-matrix of K
            sparseKvalue[sparseMapM[e][i][j] + nnzM]  += Ke_11[i][j];   // Assemble middle sub-matrix of K
            sparseKvalue[sparseMapM[e][i][j] + nnzM2] += Ke_11[i][j];   // Assemble lower right sub-matrix of K
         }
      }
     
      // Assemble Ge into sparse G.
      for (int i = 0; i < NENv; i++) {
         for (int j = 0; j < NENp; j++) {
            sparseGvalue[sparseMapG[e][i][j]]         += Ge_1[i][j];   // Assemble upper part of G
            sparseGvalue[sparseMapG[e][i][j] + nnzG]  += Ge_2[i][j];   // Assemble middle of G
            sparseGvalue[sparseMapG[e][i][j] + nnzG2] += Ge_3[i][j];   // Assemble lower part of G
         }
      }
     
   }  // Element loop


   for (int i = 0; i < NENv; i++) {
      delete[] Me_11[i];
      delete[] Ke_11[i];
      delete[] Ge_1[i];
      delete[] Ge_2[i];
      delete[] Ge_3[i];
   }
   delete[] Me_11;
   delete[] Ke_11;
   delete[] Ge_1;
   delete[] Ge_2;
   delete[] Ge_3;


   //  CONTROL
//   for (int i = 0; i < sparseM_NNZ; i++){
//      cout << i+1 << "  " << sparseMrow[i]+1 << "  " << sparseMcol[i]+1 << "  " << sparseMvalue[i] << endl;
//   }
//   for (int i = 0; i < sparseM_NNZ; i++){
//      cout << i+1 << "  " << sparseKrow[i]+1 << "  " << sparseKcol[i]+1 << "  " << sparseKvalue[i] << endl;
//   }
//   for (int i = 0; i < sparseG_NNZ; i++){
//      cout << i+1 << "  " << sparseGrow[i]+1 << "  " << sparseGcol[i]+1 << "  " << sparseGvalue[i] << endl;
//   }

   
   // Find the diagonalized mass matrix
   int row;
   for (int i = 0; i < nnzM3; i++) {
      row = sparseMrow[i];
      Md[row] += sparseMvalue[i];
   }

   delete[] sparseMvalue;

   // CONTROL
   //for (int i = 0; i < 3*NN; i++) {
   //   cout << Md[i] << endl;
   //}
   


   // Get a copy of Md before modifying it for the BCs of step 1.
   for (int i = 0; i < 3*NN; i++) {
      MdOrig[i] = Md[i];
   }

   // Calculate the inverse of Md.
   for (int i = 0; i < 3*NN; i++) {
      MdOrigInv[i] = 1.0 / MdOrig[i];
   }

   // Apply velocity BCs to Md and calculate its inverse.
   applyBC_Step1(1);
   for (int i = 0; i < 3*NN; i++) {
      MdInv[i] = 1.0 / Md[i];
   }

   // CONTROL
   //for (int i = 0; i < 3*NN; i++) {
   //   cout << MdInv[i] << endl;
   //}

   // Calculate inv(Md) * K. It'll be used in step2.
   for (int i = 0; i < sparseM_NNZ; i++) {
      sparseMdOrigInvTimesKvalue[i] = sparseKvalue[i] * MdOrigInv[sparseMrow[i]];
      //cout << i << "   " << sparseMrow[i] << "   " << sparseMcol[i] << "   " << sparseMdOrigInvTimesKvalue[i] << endl;
   }



   // Use Timothy Davis' CSparse package to store matrix G and its transpose in sparse format.
   // CSparse library uses type csi instead of int. So we need to redifine some variables.

   // TODO : This is simply waste of memory. Do something to avoid this.                                                                             TODO

   //int sparseM_NNZcs;      // Counts nonzero entries in i) a single sub-mass matrix and ii) full Mass matrix.
   //csi *sparseMcolcs;      // Nonzero columns of M, K and A matrices.
   //csi *sparseMrowcs;      // Nonzero rows of M, K and A matrices.
   //int sparseG_NNZcs;      // Counts nonzero entries in i) sub-G matrix and ii) full G matrix.
   //int *sparseGcolcs;      // Nonzero columns of G matrix.
   //int *sparseGrowcs;      // Nonzero rows of G matrix.
   //sparseGcolcs = new int[3 * sparseG_NNZ];
   //sparseGrowcs = new int[3 * sparseG_NNZ];
   //sparseMcolcs = new csi[3 * sparseM_NNZ];
   //sparseMrowcs = new csi[3 * sparseM_NNZ];
   //sparseM_NNZcs = sparseM_NNZ;
   //sparseG_NNZcs = sparseG_NNZ;
   //for (int i = 0; i < sparseG_NNZ; i++) {
   //   sparseGcolcs[i] = sparseGcol[i];
   //   sparseGrowcs[i] = sparseGrow[i];
   //}
   //for (int i = 0; i < sparseM_NNZ; i++) {
   //   sparseMcolcs[i] = sparseMcol[i];
   //   sparseMrowcs[i] = sparseMrow[i];
   //}

   G_cs = cs_spalloc(3*NN, NNp, sparseG_NNZ, 0, 1);   // See page 12 of Tim Davis' book.                                                             // TODO : 4. parametre olarak 0 veya 1 vermek birseyi degistirmiyor.
                                                                                                                                                     //        Why don't we directly allocate this in compressed format?

   G_cs->i = sparseGrow;
   G_cs->p = sparseGcol;
   G_cs->x = sparseGvalue;
   G_cs->nz = sparseG_NNZ;

   G_cs_CSC = cs_compress(G_cs);             // Convert G from triplet format into compressed column format.

   Gt_cs_CSC = cs_transpose(G_cs_CSC, 1);    // Determine the transpose of G in compressed column format.

   //  CONTROL
   //cs_print(G_cs_CSC, 0);
   //cs_print(Gt_cs_CSC, 0);

   
   waitForUser("OK1. Enter a character... ");

   // Use CSparse library to calculate [Z] = dt^2 * transpose(G) * inv(Md) * G

   // First calculate dummy = inv(Md) * G. It will have the same sparsity pattern with G, only the values will change.
   double *dummyValues;
   dummyValues = new double[sparseG_NNZ];
   for (int i = 0; i < sparseG_NNZ; i++) {
      dummyValues[i] = sparseGvalue[i] * MdOrigInv[sparseGrow[i]];
   }
   // Allocate the dummy matrix in CSparse triplet format
   cs *dummy_cs;
   dummy_cs = cs_spalloc(3*NN, NNp, sparseG_NNZ, 0, 1);                                                                                              // TODO : 4. parametre olarak 0 veya 1 vermek birseyi degistirmiyor.
   dummy_cs->i = sparseGrow;
   dummy_cs->p = sparseGcol;
   dummy_cs->x = dummyValues;
   dummy_cs->nz = sparseG_NNZ;
   // Convert dummy matrix from triplet format to CSC format
   cs *dummy_cs_CSC;
   dummy_cs_CSC = cs_compress(dummy_cs);

   waitForUser("OK2. Enter a character... ");

   // Multiply transpose(G) with the dummy matrix
   Z_cs = cs_multiply(Gt_cs_CSC, dummy_cs_CSC);
   
   //cs_spfree(dummy_cs);    // If we delete this sparseGcol is also deleted, but we need it later.                                                  // TODO
   delete[] dummyValues;

   delete[] sparseGrow;
   
   cs_spfree(dummy_cs_CSC);

   // Multiply the Z matrix with dt^2
   for(int i=0; i<Z_cs->nzmax; i++) {
      Z_cs->x[i] = Z_cs->x[i] * dt*dt;
   }
   //cs_print(Z_cs, 0);

   // Apply pressure BCs to [Z]
   applyBC_Step2(1);
   
   //Apply AMD re-ordering to [Z] and calculate its Cholesky factorization.
   //csi *AMDorderOfZ;
   //AMDorderOfZ = cs_amd(1, Z_cs);
   //for(int i=0; i<Z_cs->m; i++) {
   //   cout << AMDorderOfZ[i] << endl;
   //}
   
   waitForUser("OK3. Enter a character... ");

   Z_sym = cs_schol(1, Z_cs);
   
   waitForUser("OK4. Enter a character... ");

   Z_chol = cs_chol(Z_cs, Z_sym);

   //   cs_print(Z_chol->L, 0);
   //for (int i =0; i < Z_cs->m; i++) {
   //   cout << i << "   " << Z_sym->pinv[i] << endl;
   //}

   // CONTROL
   cout << endl << "NNZ of Z = " << Z_cs->nzmax << endl;
   cout << endl << "NNZ of Z_chol->L = " << Z_chol->L->nzmax << endl;


   // Do allocations for CSparse matrices that'll be used in the following steps.
   // See page 12 of Tim Davis' book.
   //K_cs = cs_spalloc(3*NN, 3*NN, sparseM_NNZ, 1, 1);
   //K_cs->i = sparseMrow;
   //K_cs->p = sparseMcol;
   //K_cs->x = sparseKvalue;
   //K_cs->nz = sparseM_NNZ;
   //A_cs = cs_spalloc(3*NN, 3*NN, sparseM_NNZ, 1, 1);
   //A_cs->i = sparseMrow;
   //A_cs->p = sparseMcol;
   //A_cs->nz = sparseM_NNZ;


   // Deallocate memory                                                                                                                              // TODO
   //delete[] sparseGcolcs;
   //delete[] sparseGcolcs;
   //delete[] sparseMcolcs;
   //delete[] sparseMrowcs;
   delete[] Md;
   delete[] MdOrigInv;
   cs_spfree(Z_cs);

   


/* CSparse Exercise

   csi *Ai, *Aj;
   double *Ax;

   Ai = new csi[4];
   Aj = new csi[4];
   Ax = new double[10];

   Ai[0] = 0;   Aj[0] = 0;   Ax[0] = 4.5;
   Ai[1] = 1;   Aj[1] = 0;   Ax[1] = 3.1;
   Ai[2] = 3;   Aj[2] = 0;   Ax[2] = 3.5;
   Ai[3] = 1;   Aj[3] = 1;   Ax[3] = 2.9;
   Ai[4] = 2;   Aj[4] = 1;   Ax[4] = 1.7;
   Ai[5] = 3;   Aj[5] = 1;   Ax[5] = 0.4;
   Ai[6] = 0;   Aj[6] = 2;   Ax[6] = 3.2;
   Ai[7] = 2;   Aj[7] = 2;   Ax[7] = 3.0;
   Ai[8] = 1;   Aj[8] = 3;   Ax[8] = 0.9;
   Ai[9] = 3;   Aj[9] = 3;   Ax[9] = 1.0;

   csi m = 4;
   csi n = 4;
   csi nzmax = 10;
   csi values = 1;
   csi triplet = 1;
 
   cs *A_cs;
   A_cs = cs_spalloc(m, n, nzmax, values, triplet);

   A_cs->i = Ai;
   A_cs->p = Aj;
   A_cs->x = Ax;
   A_cs->nz = 10;

   cs *B_cs;
   B_cs = cs_compress(A_cs);
   cs_print(A_cs, 0);
   cs_print(B_cs, 0);

   //cs_spfree(A_cs);
   //cs_spfree(B_cs);
*/


/* CUSP Exercise

   int *rowIndices;
   int *colIndices;
   float *values;

   int N = 3;
   int NNZ = 3;
   rowIndices = new int[N];
   colIndices = new int[N];
   values = new float[NNZ];

   // initialize matrix entries on host
   rowIndices[0] = 0; colIndices[0] = 2; values[0] = 10;
   rowIndices[1] = 1; colIndices[1] = 0; values[1] = 20;
   rowIndices[2] = 2; colIndices[2] = 0; values[2] = 30;

  // A now represents the following matrix
  //    [ 0  0  10]
  //    [20  0   0]
  //    [30  0   0]

  cusp::coo_matrix<int,float,cusp::host_memory> A(N,N,NNZ);
  thrust::copy(rowIndices, rowIndices + N, A.row_indices.begin());
  thrust::copy(colIndices, colIndices + N, A.column_indices.begin());
  thrust::copy(values, values + NNZ, A.values.begin());

  cusp::coo_matrix<int,float,cusp::device_memory> B(N,N,NNZ);
  thrust::copy(rowIndices, rowIndices + N, B.row_indices.begin());
  thrust::copy(colIndices, colIndices + N, B.column_indices.begin());
  thrust::copy(values, values + NNZ, B.values.begin());

  cusp::csr_matrix<int,float,cusp::device_memory> Acsr(A);
  cusp::csr_matrix<int,float,cusp::device_memory> Bcsr(B);

  cusp::coo_matrix<int,float,cusp::device_memory> C;
  cusp::multiply(Acsr, Bcsr, C);
  cusp::print(C);

*/

}  // End of function step0()





//========================================================================
void step1(int iter)
//========================================================================
{
   // Executes step 1 of the method to determine the intermediate velocity.

   // Calculate Ae and assemble into A. do this only for the first iteration of each time step.
   if (iter == 1) {

      int nnzM = sparseM_NNZ / 3;
      int nnzM2 = 2 * nnzM;

      double *u0_nodal, *v0_nodal, *w0_nodal;
      double u0, v0, w0;
   
      u0_nodal = new double[NENv];
      v0_nodal = new double[NENv];
      w0_nodal = new double[NENv];

      double **Ae_11;
      double GQfactor;

      for (int i = 0; i < sparseM_NNZ; i++){
         sparseAvalue[i] = 0.0;
      }

      Ae_11 = new double*[NENv];
      for (int i = 0; i < NENv; i++) {
         Ae_11[i] = new double[NENv];
      }

      // Calculate Ae and assemble it into A
      for (int e = 0; e < NE; e++) {

         for (int i = 0; i < NENv; i++) {
            for (int j = 0; j < NENv; j++) {
               Ae_11[i][j] = 0.0;
            }
         }
         
         // Extract elemental u, v and w velocity values from the global solution
         // solution array of the previous iteration.
         int iG;
         for (int i = 0; i<NENv; i++) {
            iG = LtoGvel[e][i];
            u0_nodal[i] = Un[iG];
  
            iG = LtoGvel[e][i + NENv];
            v0_nodal[i] = Un[iG];
      
            iG = LtoGvel[e][i + 2*NENv];
            w0_nodal[i] = Un[iG];
         }

         for (int k = 0; k < NGP; k++) {   // Gauss Quadrature loop
            GQfactor = detJacob[e][k] * GQweight[k];

            // Above calculated u0 and v0 values are at the nodes. However in GQ
            // integration we need them at GQ points. Let's calculate them using
            // interpolation based on shape functions.
            u0 = 0.0;
            v0 = 0.0;
            w0 = 0.0;
            for (int i = 0; i<NENv; i++) {
               u0 = u0 + Sv[i][k] * u0_nodal[i];
               v0 = v0 + Sv[i][k] * v0_nodal[i];
               w0 = w0 + Sv[i][k] * w0_nodal[i];
            }
       
            for (int i = 0; i < NENv; i++) {
               for (int j = 0; j < NENv; j++) {
                  Ae_11[i][j] = Ae_11[i][j] + (u0 * gDSv[e][0][j][k] + v0 * gDSv[e][1][j][k] + w0 * gDSv[e][2][j][k]) * Sv[i][k] * GQfactor;
               }
            }       
         } // GQ loop
     
     
         // Assemble Ae into sparse A.
         for (int i = 0; i < NENv; i++) {
            for (int j = 0; j < NENv; j++) {
               sparseAvalue[sparseMapM[e][i][j]]         += Ae_11[i][j];   // Assemble upper left sub-matrix of A
               sparseAvalue[sparseMapM[e][i][j] + nnzM]  += Ae_11[i][j];   // Assemble middle sub-matrix of A
               sparseAvalue[sparseMapM[e][i][j] + nnzM2] += Ae_11[i][j];   // Assemble lower right sub-matrix of A
            }
         }
      }  // End of element loop


      for (int i = 0; i < NENv; i++) {
         delete[] Ae_11[i];
      }
      delete[] Ae_11;

      delete[] u0_nodal;
      delete[] v0_nodal;
      delete[] w0_nodal;

      //  CONTROL
      //for (int i = 0; i < sparseM_NNZ; i++){
      //   cout << i+1 << "  " << sparseMrow[i]+1 << "  " << sparseMcol[i]+1 << "  " << sparseAvalue[i] << endl;
      //}

      //A_cs->x = sparseAvalue;
      //A_cs_CSC = cs_compress(A_cs);             // Convert A from triplet format into compressed column format.

   }  // End of iter==1 check


   
   // Calculate the RHS vector of step 1.
   // R1 = - K * UnpHalf_prev - A * UnpHalf_prev - G * Pn;

   //double *dummyR1;
   //dummyR1 = double[3*NN];
   //for(int)

   char transa, matdescra[6];
   double alpha = -1.0;
   double beta = 0.0;    // To add the calculated R1 to the previosuly calculated one.
   int m = 3*NN;
   int k = NNp;
   int *pointerE;
   int *pointerE2;
   pointerE = new int[m];
   pointerE2 = new int[m];

   transa = 'n';
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   for (int i = 0; i < m; i++) {
      pointerE[i] = sparseMrowIndex[i+1];   // A new form of rowIndex data required by mkl_dcsrmv
   };
   for (int i = 0; i < m; i++) {
      pointerE2[i] = sparseGrowIndex[i+1];  // A new form of rowIndex data required by mkl_dcsrmv
   };

   mkl_dcsrmv(&transa, &m, &m, &alpha, matdescra, sparseKvalue, sparseMcol, sparseMrowIndex, pointerE, UnpHalf_prev, &beta, R1);   // This is (- K * UnpHalf_prev)  part of R1
   beta = 1.0;    // To add the calculated R1 to the previosuly calculated one.
   mkl_dcsrmv(&transa, &m, &m, &alpha, matdescra, sparseAvalue, sparseMcol, sparseMrowIndex, pointerE, UnpHalf_prev, &beta, R1);   // This is (- A * UnpHalf_prev)  part of R1
   mkl_dcsrmv(&transa, &m, &k, &alpha, matdescra, sparseGvalue, sparseGcol, sparseGrowIndex, pointerE2, Pn, &beta, R1);            // This is (- G * Pn          )  part of R1

   // CONTROL
   //for (int i = 0; i < m; i++) {
   //   cout << R1[i] << endl;
   //}
   

   // Modify R1 for velocity BCs
   applyBC_Step1(2);

   // Calculate AccHalf vector.
   for (int i=0; i<3*NN; i++) {
      AccHalf[i] = R1[i] * MdInv[i];
   }
   
   // Calculate UnpHalf
   for (int i=0; i<3*NN; i++) {
      UnpHalf[i] = Un[i] + dt * AccHalf[i];
   }

   // CONTROL
   //for (int i=0; i<3*NN; i++) {
   //   cout << UnpHalf[i] << endl;
   //}

   delete[] pointerE;
   delete[] pointerE2;


   /* MKL Sparse Matrix Vector Multiplication Test (Extracted from MKL's cspblas_dcs.c example code)

   #define M 5
   #define NNZ 13

   double values[NNZ] = {1.0, -1.0, -3.0, -2.0, 5.0, 4.0, 6.0, 4.0, -4.0, 2.0, 7.0, 8.0, -5.0};
   int columns[NNZ]   = {0, 1, 3, 0, 1, 2, 3, 4, 0, 2, 3, 1, 4};
   int rowIndex[M+1]  = {0, 3,  5,  8,  11, 13};
   double x_vec[M]	 = {1.0, 1.0, 1.0, 1.0, 1.0};
   double y_vec[M]	 = {0.0, 0.0, 0.0, 0.0, 0.0};

   char transa, matdescra[6];
   double alpha = 1.0, beta = 0.0;
   int m, k;
   int pointerB[M], pointerE[M];

   transa = 'n';
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   m = M;
   
   for (int i = 0; i < m; i++) {
      pointerB[i] = rowIndex[i];
      pointerE[i] = rowIndex[i+1];
   };
   
   mkl_dcsrmv(&transa, &m, &m, &alpha, matdescra, values, columns, pointerB, pointerE, x_vec, &beta, y_vec);

   for (int i = 0; i < m; i++) {
      printf("%7.1f\n", y_vec[i]);
   }

   */

}  // End of function step1()





//========================================================================
void step2()
//========================================================================
{
   // Executes step 2 of the method to determine pressure of the new time step.

   // Calculate the RHS vector of step 2.
   // R2 = Gt * (UnpHalf - dt*dt * MdOrigInv * K * Acc_prev)
   
   double *dummyR2;
   dummyR2 = new double[3*NN];
   
   for (int i=0; i<3*NN; i++) {
      dummyR2[i] = UnpHalf[i];
   }

   char transa, matdescra[6];
   double alpha = -dt*dt;
   double beta = 1.0;          // Add the calculated dummyR2 to the previous dummyR2
   int m = 3*NN;
   int *pointerE;
   pointerE = new int[m];

   transa = 'n';
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   for (int i = 0; i < m; i++) {
      pointerE[i] = sparseMrowIndex[i+1];   // A new form of rowIndex data required by mkl_dcsrmv
   };

   mkl_dcsrmv(&transa, &m, &m, &alpha, matdescra, sparseKvalue, sparseMcol, sparseMrowIndex, pointerE, Acc_prev, &beta, dummyR2);   // This is (-dt*dt * K * Acc_prev)  part of R2. It is also added to UnpHalf.


   alpha = 1.0;
   beta = 0.0;
   m = 3*NN;
   int k = NNp;
   transa = 't';    // Multiply using Gt, not G
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   for (int i = 0; i < m; i++) {
      pointerE[i] = sparseGrowIndex[i+1];   // A new form of rowIndex data required by mkl_dcsrmv
   };

   mkl_dcsrmv(&transa, &m, &k, &alpha, matdescra, sparseGvalue, sparseGcol, sparseGrowIndex, pointerE, dummyR2, &beta, R2);   // This is (Gt * dummyR2) which gives R2

   // CONTROL
   //for (int i=0; i<NNp; i++) {
   //   cout << R2[i] << endl;
   //}


   // Apply BCs for step2. Modify R2 for pressure BCs.
   applyBC_Step2(2);



   // Solve for Pdot using Cholesky factorization obtained in step 0.
   // Reference: Timothy Davis' Book, page 136
   for (int i=0; i<NNp; i++) {
      Pdot[i] = R2[i];                    // Equate the solution vector the the RHS vector first.
   }
   double *x;
   x = new double[NNp]; //cs_malloc(NNp, sizeof(double));    // Get workspace
   cs_ipvec(Z_sym->pinv, R2, x, NNp);
   cs_lsolve(Z_chol->L, x);
   cs_ltsolve(Z_chol->L, x);
   cs_pvec(Z_sym->pinv, x, Pdot, NNp);
   cs_free(x);

   // CONTROL
   //for (int i=0; i<NNp; i++) {
   //   cout << Pdot[i] << endl;
   //}


   // Calculate Pnp1
   for (int i=0; i<NNp; i++) {
      Pnp1[i] = Pn[i] + dt * Pdot[i];
   }
   
   // CONTROL
   //for (int i=0; i<NNp; i++) {
   //   cout << Pnp1[i] << endl;
   //}

   delete[] dummyR2;
   delete[] pointerE;

}  // End of function step2()





//========================================================================
void step3()
//========================================================================
{
   // Executes step 3 of the method to determine the velocity of the new time step.

   // Calculate the RHS vector of step 3.
   // R3 = - dt * (G * Pdot + K * Acc_prev);

   char transa, matdescra[6];
   double alpha = -dt;
   double beta = 0.0;
   int m = 3*NN;
   int *pointerE; 
   pointerE = new int[m];

   transa = 'n';
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   for (int i = 0; i < m; i++) {
      pointerE[i] = sparseMrowIndex[i+1];   // A new form of rowIndex data required by mkl_dcsrmv
   };

   mkl_dcsrmv(&transa, &m, &m, &alpha, matdescra, sparseKvalue, sparseMcol, sparseMrowIndex, pointerE, Acc_prev, &beta, R3);   // This is (-dt * K * Acc_prev)  part of R3.


   alpha = -dt;
   beta = 1.0;     // Add the the previously calculated R3
   m = 3*NN;
   int k = NNp;
   transa = 'n';
   
   matdescra[0] = 'g';
   matdescra[1] = 'u';
   matdescra[2] = 'n';
   matdescra[3] = 'c';
   
   for (int i = 0; i < m; i++) {
      pointerE[i] = sparseGrowIndex[i+1];   // A new form of rowIndex data required by mkl_dcsrmv
   };

   mkl_dcsrmv(&transa, &m, &k, &alpha, matdescra, sparseGvalue, sparseGcol, sparseGrowIndex, pointerE, Pdot, &beta, R3);   // This is (-dt * G * Pdot) part of R3. It is added to the preiously calculated part.

   // CONTROL
   //for (int i=0; i<3*NN; i++) {
   //   cout << R3[i] << endl;
   //}


   // Modify R3 for velocity BCs
   applyBC_Step3();

   // Calculate Acc vector.
   for (int i=0; i<3*NN; i++) {
      Acc[i] = R3[i] * MdInv[i];
   }

   // Calculate Unp
   for (int i=0; i<3*NN; i++) {
      Unp1[i] = UnpHalf[i] + dt * Acc[i];
   }

   // CONTROL
   //for (int i=0; i<3*NN; i++) {
   //   cout << Unp1[i] << endl;
   //}

   delete[] pointerE;

}  // End of function step3()





//========================================================================
void applyBC_initial()
//========================================================================
{
   // Apply the specified BCs before the solution starts.
   //double x, y, z;
   int node, whichBC;

   // Apply velocity BCs
   for (int i = 0; i < BCnVelNodes; i++) {
      node = BCvelNodes[i][0];     // Node at which this velocity BC is specified.
      whichBC = BCvelNodes[i][1];  // Number of the specified BC

      //x = coord[node][0];           // May be necessary for BC.str evaluation
      //y = coord[node][1];
      //z = coord[node][2];
     
      // Change Un with the given u, v and w velocities.
      Un[node]        = BCstr[whichBC][0];                                                                                                           // TODO : Actually BCstr should be strings and here we need a function parser.
      Un[node + NN]   = BCstr[whichBC][1];
      Un[node + 2*NN] = BCstr[whichBC][2];
   }

   /* CONTROL
   for (int i = 0; i < 3*NN; i++) {
      cout << Un[i] << endl;
   }
   */

}  // End of function applyBC_initial()





//========================================================================
void applyBC_Step1(int flag)
//========================================================================
{
   // When flag=1, modify Md for velocity BCs. When flag=2, modify the right
   // hand side vector of step 1 (R1) for velocity BCs.

   // WARNING : In step 1 velocity differences between 2 iterations is
   // calculated. Therefore when specifying velocity BCs a value of zero is
   // specified instead of the original velocity value.

   int node;

   if (flag == 1) {
      for (int i = 0; i < BCnVelNodes; i++) {
         node = BCvelNodes[i][0];   // Node at which this velocity BC specified.
         Md[node]        = 1.0;     // Change Md for the given u velocity.
         Md[node + NN]   = 1.0;     // Change Md for the given v velocity.
         Md[node + 2*NN] = 1.0;     // Change Md for the given w velocity.
      }

   } else if (flag == 2) {
      for (int i = 0; i < BCnVelNodes; i++) {
         node = BCvelNodes[i][0];   // Node at which this velocity BC is specified.
     
         // Change R1 for the given u and v velocities.
         R1[node]        = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
         R1[node + NN]   = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
         R1[node + 2*NN] = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
      }
   }
}  // End of function applyBC_Step1()





//========================================================================
void applyBC_Step2(int flag)
//========================================================================
{
   // When flag=1, modify Z for pressure BCs. When flag=2, modify the right
   // hand side vector of step 2 (R2) for pressure BCs.

   // WARNING : In step 2 pressure differences between 2 iterations is
   // calculated. Therefore when specifying pressure BCs a value of zero is
   // specified instead of the original pressure value.

   // In order not to break down the symmetry of [Z], we use the "LARGE number"
   // trick.

   double LARGE = 1000;                                                                                                                              // TODO: Implement EBCs without the use of LARGE.

   int node = zeroPressureNode;     // Node at which pressure is set to zero.

   if (flag == 1) {
      if (node > 0) {  // If node is negative it means we do not set pressure to zero at any node.
         // Multiply Z[node][node] by LARGE
		 for (int j = Z_cs->p[node]; j < Z_cs->p[node+1]; j++) {  // Go through column "node" of [Z].
            if (Z_cs->i[j] == node) {   // Determine the position of the diagonal entry in column "node"
               Z_cs->x[j] = Z_cs->x[j] * LARGE;
               break;
            }
         }
      }
   } else if (flag == 2) {
      if (node > 0) {  // If node is negative it means we do not set pressure to zero at any node.
         R2[node] = 0.0;  // This is not pressure, but pressure difference between 2 iterations.
      }
   }
}  // End of function applyBC_Step2()




//========================================================================
void applyBC_Step3()
//========================================================================
{
   // Modify the right hand side vector of step 3 (R3) for velocity BCs.
   
   int node;

   for (int i = 0; i < BCnVelNodes; i++) {
      node = BCvelNodes[i][0];   // Node at which this velocity BC specified.
  
     // Change R3 for the given u and v velocities.
     R3[node]        = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
     R3[node + NN]   = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
     R3[node + 2*NN] = 0.0;  // This is not velocity, but velocity difference between 2 iterations.
   }
}  // End of function applyBC_Step3()




//========================================================================
void readRestartFile()
//========================================================================
{

   // TODO ...

}  // End of function readRestartFile()





//========================================================================
void createTecplot()
//========================================================================
{
   // This file is used if NENp and NENv are different. In this case each
   // hexahedral element is divided into eight sub-elements and Tecplot file
   // is created as if there are 8*NE hexahedral elements. Missing mid-edge,
   // mid-face and mid-element pressure values are evaluated by linear
   // interpolation.

   // Call the simple version of this function if NENp and NENv are the same.

   int node, n1, n2, n3, n4, n5, n6, n7, n8;

   // Write the calculated unknowns to a Tecplot file
   datFile.open((whichProblem + ".dat").c_str(), ios::out);

   datFile << "TITLE = " << whichProblem << endl;
   datFile << "VARIABLES = x,  y,  z,  u, v, w, p" << endl;
   
   if (eType == 1) {
      datFile << "ZONE N=" <<  NN  << ", E=" << 8*NE << ", F=FEPOINT, ET=BRICK" << endl;
      // New Tecplot 360 documentation has the following format but the above seems to be working also
      // ZONE NODES=..., ELEMENTS=..., DATAPACKING=POINT, ZONETYPE=FEBRICK
   } else {
      printf("\n\n\nERROR: Tetrahedral elements are not implemented in function createTecplot() yet!!!\n\n\n");
   }

   // Separate Un into uNode, vNode and wNode variables
   double *uNode, *vNode, *wNode;
   uNode = new double[NN];
   vNode = new double[NN];
   wNode = new double[NN];

   for (int i = 0; i < NN; i++) {
      uNode[i] = Un[i];
      vNode[i] = Un[i+NN];
      wNode[i] = Un[i+2*NN];
   }

   // Copy pressure solution into pNode array, but the size of pNode is NN,
   // because it will also store pressure values at mid-egde, mid-face and
   // mid-element nodes.
   double *pNode;
   pNode = new double[NN];

   for (int i = 0; i < NNp; i++) {
      pNode[i] = Pn[i];
   }

   // Interpolate pressure at non-corner nodes.
   for (int e = 0; e < NE; e++) {
      // Calculate mid-edge pressures as averages of the corner pressures.
      for (int ed = 0; ed < NEE; ed++) {
         // Determine corner nodes of edge ed
         if (eType == 1) {   // Hexahedral element
            switch (ed) {
            case 0:
              n1 = LtoGnode[e][0];
              n2 = LtoGnode[e][1];
              break;
            case 1:
              n1 = LtoGnode[e][1];
              n2 = LtoGnode[e][2];
              break;
            case 2:
              n1 = LtoGnode[e][2];
              n2 = LtoGnode[e][3];
              break;
            case 3:
              n1 = LtoGnode[e][3];
              n2 = LtoGnode[e][0];
              break;
            case 4:
              n1 = LtoGnode[e][0];
              n2 = LtoGnode[e][4];
              break;
            case 5:
              n1 = LtoGnode[e][1];
              n2 = LtoGnode[e][5];
              break;
            case 6:
              n1 = LtoGnode[e][2];
              n2 = LtoGnode[e][6];
              break;
            case 7:
              n1 = LtoGnode[e][3];
              n2 = LtoGnode[e][7];
              break;
            case 8:
              n1 = LtoGnode[e][4];
              n2 = LtoGnode[e][5];
              break;
            case 9:
              n1 = LtoGnode[e][5];
              n2 = LtoGnode[e][6];
              break;
            case 10:
              n1 = LtoGnode[e][6];
              n2 = LtoGnode[e][7];
              break;
            case 11:
              n1 = LtoGnode[e][7];
              n2 = LtoGnode[e][4];
              break;
            }  // End of ed switch
         } else if (eType == 2) {   // Tetrahedral element
           printf("\n\n\nERROR: Tetrahedral elements are not implemented in function createTecplot() yet!!!\n\n\n");
         }
         
         node = LtoGnode[e][ed+NEC];

         pNode[node] = 0.5 * (pNode[n1] + pNode[n2]);

      }  // End of ed (edge) loop


      // Calculate mid-face pressures as averages of the corner pressures.
      for (int f = 0; f < NEF; f++) {
  
         // Determine corner nodes of face f
         if (eType == 1) {   // Hexahedral element
            switch (f) {
            case 0:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][1];
               n3 = LtoGnode[e][2];
               n4 = LtoGnode[e][3];
               break;
            case 1:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][1];
               n3 = LtoGnode[e][4];
               n4 = LtoGnode[e][5];
               break;
            case 2:
               n1 = LtoGnode[e][1];
               n2 = LtoGnode[e][2];
               n3 = LtoGnode[e][5];
               n4 = LtoGnode[e][6];
               break;
            case 3:
               n1 = LtoGnode[e][2];
               n2 = LtoGnode[e][3];
               n3 = LtoGnode[e][6];
               n4 = LtoGnode[e][7];
               break;
            case 4:
               n1 = LtoGnode[e][0];
               n2 = LtoGnode[e][3];
               n3 = LtoGnode[e][4];
               n4 = LtoGnode[e][7];
               break;
            case 5:
               n1 = LtoGnode[e][4];
               n2 = LtoGnode[e][5];
               n3 = LtoGnode[e][6];
               n4 = LtoGnode[e][7];
               break;
            }

            node = LtoGnode[e][f+NEC+NEE];
            pNode[node] = 0.25 * (pNode[n1] + pNode[n2] + pNode[n3] + pNode[n4]);
         
         } else if (eType == 2) {   // Tetrahedral element
            printf("\n\n\nERROR: Tetrahedral elements are not implemented in function createTecplot() yet!!!\n\n\n");
         }
      }  // End of f (face) loop


      // Find add the mid-element node pressures.
      if (eType == 1) {   // Hexahedral element
         n1 = LtoGnode[e][0];
         n2 = LtoGnode[e][1];
         n3 = LtoGnode[e][2];
         n4 = LtoGnode[e][3];
         n5 = LtoGnode[e][4];
         n6 = LtoGnode[e][5];
         n7 = LtoGnode[e][6];
         n8 = LtoGnode[e][7];
    
         node = LtoGnode[e][NEC+NEE+NEF];
    
         pNode[node] = 0.125 * (pNode[n1] + pNode[n2] + pNode[n3] + pNode[n4] + pNode[n5] + pNode[n6] + pNode[n7] + pNode[n8]);

      } else if (eType == 2) {   // Tetrahedral element
         printf("\n\n\nERROR: Tetrahedral elements are not implemented in function createTecplot() yet!!!\n\n\n");
      }
   }  // End of element loop


   // Print the coordinates and the calculated velocity and pressure values
   double x, y, z;
   for (int i = 0; i < NN; i++) {
      x = coord[i][0];
      y = coord[i][1];
      z = coord[i][2];
      datFile.precision(11);
      datFile << scientific << x << " " << y << " " << z << " " << uNode[i] << " " << vNode[i] << " " << wNode[i] << " " << pNode[i] << endl;
   }


   // Print the connectivity list. We will divide hexahedral elements into 8
   // and divide tetrahedral elements into ... elements.                                                                                             TODO
   if (eType == 1) {   // Hexahedral elements
      for (int e = 0; e < NE; e++) {
         // 1st sub-element of element e
         datFile << LtoGnode[e][0] + 1 << " " << LtoGnode[e][8] + 1 << " " << LtoGnode[e][20] + 1 << " " << LtoGnode[e][11] + 1 << " " << LtoGnode[e][12] + 1 << " " << LtoGnode[e][21] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][24] + 1 << endl;
         // 2nd sub-element of element e
         datFile << LtoGnode[e][8] + 1 << " " << LtoGnode[e][1] + 1 << " " << LtoGnode[e][9] + 1 << " " << LtoGnode[e][20] + 1 << " " << LtoGnode[e][21] + 1 << " " << LtoGnode[e][13] + 1 << " " << LtoGnode[e][22] + 1 << " " << LtoGnode[e][26] + 1 << endl;
         // 3rd sub-element of element e
         datFile << LtoGnode[e][11] + 1 << " " << LtoGnode[e][20] + 1 << " " << LtoGnode[e][10] + 1 << " " << LtoGnode[e][3] + 1 << " " << LtoGnode[e][24] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][23] + 1 << " " << LtoGnode[e][15] + 1 << endl;
         // 4th sub-element of element e
         datFile << LtoGnode[e][20] + 1 << " " << LtoGnode[e][9] + 1 << " " << LtoGnode[e][2] + 1 << " " << LtoGnode[e][10] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][22] + 1 << " " << LtoGnode[e][14] + 1 << " " << LtoGnode[e][23] + 1 << endl;
         // 5th sub-element of element e
         datFile << LtoGnode[e][12] + 1 << " " << LtoGnode[e][21] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][24] + 1 << " " << LtoGnode[e][4] + 1 << " " << LtoGnode[e][16] + 1 << " " << LtoGnode[e][25] + 1 << " " << LtoGnode[e][19] + 1 << endl;
         // 6th sub-element of element e
         datFile << LtoGnode[e][21] + 1 << " " << LtoGnode[e][13] + 1 << " " << LtoGnode[e][22] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][16] + 1 << " " << LtoGnode[e][5] + 1 << " " << LtoGnode[e][17] + 1 << " " << LtoGnode[e][25] + 1 << endl;
         // 7th sub-element of element e
         datFile << LtoGnode[e][24] + 1 << " " << LtoGnode[e][26] + 1 << " " << LtoGnode[e][23] + 1 << " " << LtoGnode[e][15] + 1 << " " << LtoGnode[e][19] + 1 << " " << LtoGnode[e][25] + 1 << " " << LtoGnode[e][18] + 1 << " " << LtoGnode[e][7] + 1 << endl;
         // 8th sub-element of element e
         datFile << LtoGnode[e][26] + 1 << " " << LtoGnode[e][22] + 1 << " " << LtoGnode[e][14] + 1 << " " << LtoGnode[e][23] + 1 << " " << LtoGnode[e][25] + 1 << " " << LtoGnode[e][17] + 1 << " " << LtoGnode[e][6] + 1 << " " << LtoGnode[e][18] + 1 << endl;
      }
   } else if (eType == 2) {  // Tetrahedral elements
      printf("\n\n\nERROR: Tetrahedral elements are not implemented in function createTecplot() yet!!!\n\n\n");
   }

   delete[] uNode;
   delete[] vNode;
   delete[] wNode;
   delete[] pNode;

   datFile.close();

}  // End of function createTecplot()





//-----------------------------------------------------------------------------
double getHighResolutionTime(void)
//-----------------------------------------------------------------------------
{
   #ifdef WIN32
      // Windows
      return double(clock());    // On Windows, clock() returns wall clock time.
                                 // On Linux it returns CPU time.
   #else
      // Linux
      struct timeval tod;

      gettimeofday(&tod, NULL);  // Measures wall clock time
      double time_seconds = (double) tod.tv_sec + ((double) tod.tv_usec / 1000000.0);
      return time_seconds;
   #endif // WIN32
} // End of function getHighResolutionTime()





//-----------------------------------------------------------------------------
void waitForUser(string str)
//-----------------------------------------------------------------------------
{
   // Used for checking memory usage. Prints the input string to the screen and
   // waits for the user to enter a character.

   char dummyUserInput;
   cout << str;
   cin >> dummyUserInput;
}




























/*


//------------------------------------------------------------------------------
void calcKeKMap()
//------------------------------------------------------------------------------
{
   // This function calculates the KeKMap.
   // KeKMap keeps elemental to global stiffness matrix's mapping data for all elements.


   //-----------Ke to K map selection----------------------------
   //KeKMap : costs memory([NE][NENv][NENv]*4byte), runs faster. (default)
   //KeKMapSmall : negligible memory, runs slower. Steps to use;
   //               (1) Comment out KeKMapUSE parts
   //               (2) Uncomment KeKMapSmallUSE parts
   //------------------------------------------------------------   
   
   //-----KeKMapUSE-----
   int *eLtoG, loc, colCounter, k, e, i, j;   
   
   KeKMap = new int**[NE];                //Keeps elemental to global stiffness matrix's mapping data
   for(e=0; e<NE; e++) {                  //It costs some memory([NE][NENv][NENv]*4byte) but makes assembly function runs faster.
      KeKMap[e] = new int*[NENv];
      for(j=0; j<NENv; j++) {
         KeKMap[e][j] = new int[NENv];
      }
   }
   
   eLtoG = new int[NENv];           // elemental LtoG data    
   
   for(e=0; e<NE; e++) {

      for(k=0; k<NENv; k++) {
         eLtoG[k] = (LtoG[e][k]);      // Takes node data from LtoG
      } 

      for(i=0; i<NENv; i++) {
         for(j=0; j<NENv; j++) {
            colCounter=0;
            for(loc=rowStartsSmall[eLtoG[i]]; loc<rowStartsSmall[eLtoG[i]+1]; loc++) {  // loc is the location of the col vector(col[x], loc=x) 
               if(colSmall[loc] == eLtoG[j]) {                                         // Selection process of the KeKMapSmall data from the col vector
                  KeKMap[e][i][j] = colCounter; 
                  break;
               }
               colCounter++;
            }
         }
      }   
   }   // End of element loop 
   
   delete[] eLtoG;
   //-----KeKMapUSE-----

} // End of function calcKeKMap()






//------------------------------------------------------------------------------
void vectorProduct()
//------------------------------------------------------------------------------
{
   // This function makes calculations for some matrix-vector operations on GPU
   // Some operations at RHS of the mass-adjust velocity equations and
   // some operations at RHS of the momentum equations.
   // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4b], [4c] [4e], [4f])   
   
   cusparseHandle_t handle = 0;
   cusparseStatus_t status;
   status = cusparseCreate(&handle);
   if (status != CUSPARSE_STATUS_SUCCESS) {
      fprintf( stderr, "!!!! CUSPARSE initialization error\n" );
   }
   cusparseMatDescr_t descr = 0;
   status = cusparseCreateMatDescr(&descr); 
   if (status != CUSPARSE_STATUS_SUCCESS) {
      fprintf( stderr, "!!!! CUSPARSE cusparseCreateMatDescr error\n" );
   } 

   cusparseSetMatType(descr,CUSPARSE_MATRIX_TYPE_GENERAL);
   cusparseSetMatIndexBase(descr,CUSPARSE_INDEX_BASE_ZERO);
   
   cudaMalloc((void**)&d_col, (NNZ)*sizeof(int));
   cudaMalloc((void**)&d_row, (NN+1)*sizeof(int));
   cudaMalloc((void**)&d_val, (NNZ)*sizeof(real));
   cudaMalloc((void**)&d_x, NN*sizeof(real));  
   cudaMalloc((void**)&d_r, NN*sizeof(real));

   cudaMemcpy(d_col, colSmall, (NNZ)*sizeof(int), cudaMemcpyHostToDevice);  
   cudaMemcpy(d_row, rowStartsSmall, (NN+1)*sizeof(int), cudaMemcpyHostToDevice);  
   
   switch (vectorOperationNo) {
   
   case 1:
      // Calculate part of a RHS of the momentum equations
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4e], [4f])
      // f_x = - K_uv*v - K_uw*w   
      // f_y = - K_vu*u - K_vw*w
      // f_z = - K_wu*u - K_wv*v
      cudaMalloc((void**)&d_rTemp, NN*sizeof(real));  
   
      switch (phase) {
         case 0:
            cudaMemcpy(d_val, K_uv, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);          
            cudaMemcpy(d_x, v, NN*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 1:
            cudaMemcpy(d_val, K_vu, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);             
            cudaMemcpy(d_x, u, NN*sizeof(real), cudaMemcpyHostToDevice);
            break;
         case 2:
            cudaMemcpy(d_val, K_wu, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);  
            cudaMemcpy(d_x, u, NN*sizeof(real), cudaMemcpyHostToDevice);
            break;
      }
      
      #ifdef SINGLE
         cusparseScsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_rTemp);
      #else
         cusparseDcsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_rTemp);
      #endif
      
      switch (phase) {
         case 0:
            cudaMemcpy(d_val, K_uw, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);           
            cudaMemcpy(d_x, w, NN*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 1:
            cudaMemcpy(d_val, K_vw, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);  
            cudaMemcpy(d_x, w, NN*sizeof(real), cudaMemcpyHostToDevice);
            break;
         case 2:
            cudaMemcpy(d_val, K_wv, (NNZ)*sizeof(real), cudaMemcpyHostToDevice);  
            cudaMemcpy(d_x, v, NN*sizeof(real), cudaMemcpyHostToDevice);
            break;
      }
      
      #ifdef SINGLE
         cusparseScsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #else
         cusparseDcsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #endif      
      
      #ifdef SINGLE
         cublasSaxpy(NN,1.0,d_r,1,d_rTemp,1);
      #else
         cublasDaxpy(NN,1.0,d_r,1,d_rTemp,1);
      #endif   
      
      // Calculate part of a RHS of the momentum equations
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4e], [4f])
      // C_x * p^(i+1), C_y * p^(i+1), C_z * p^(i+1)
      switch (phase) {
         case 0:
            cudaMemcpy(d_val, Cx, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 1:
            cudaMemcpy(d_val, Cy, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 2:
            cudaMemcpy(d_val, Cz, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
      }    
      cudaMemcpy(d_x, p, NN*sizeof(real), cudaMemcpyHostToDevice);
      
      #ifdef SINGLE
         cusparseScsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #else
         cusparseDcsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #endif
      
      #ifdef SINGLE
         cublasSaxpy(NN,-1.0,d_rTemp,1,d_r,1);
      #else
         cublasDaxpy(NN,-1.0,d_rTemp,1,d_r,1);
      #endif  

      cudaMemcpy(F,d_r,(NN)*sizeof(real),cudaMemcpyDeviceToHost);   
      cudaFree(d_rTemp);      
      break;
      
   case 2: 
      // Calculating part of a RHS of the velocity correction
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4b], [4c])      
      // C_x * deltaP^(i+1/2), C_y * deltaP^(i+1/2), C_z * deltaP^(i+1/2)
      switch (phase) {
         case 0:
            cudaMemcpy(d_val, Cx, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 1:
            cudaMemcpy(d_val, Cy, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
         case 2:
            cudaMemcpy(d_val, Cz, (NNZ)*sizeof(real), cudaMemcpyHostToDevice); 
            break;
      }

      cudaMemcpy(d_x, delta_p, NN*sizeof(real), cudaMemcpyHostToDevice);
      
      #ifdef SINGLE
         cusparseScsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #else
         cusparseDcsrmv(handle,CUSPARSE_OPERATION_NON_TRANSPOSE,NN,NN,1.0,descr,d_val,d_row,d_col,d_x,0.0,d_r);
      #endif      
      cudaMemcpy(F,d_r,(NN)*sizeof(real),cudaMemcpyDeviceToHost);
      break;
   }      
   
   cudaFree(d_col);
   cudaFree(d_row);
   cudaFree(d_val);
   cudaFree(d_x);
   cudaFree(d_r);

} // End of function vectorProduct()




//------------------------------------------------------------------------------
void solve()
//------------------------------------------------------------------------------
{
   // This function is for nonlinear iterations
   // Overall structure of the function;
   // while(iteration number < maximum non-linear iterations)
   //   calculate STEP 1 (solve SCPE for pressure correction, get delta_p)
   //   calculate STEP 2 (mass-adjust velocity field and increment pressure, get u^(i+1/2), v^(i+1/2), w^(i+1/2) & p^(i+1))
   //   calculate STEP 3 (solve x, y, z momentum equations, get u^(i+1), v^(i+1), w^(i+1))
   //   check convergence
   //   print monitor points and other info
   // end
   //
   // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4a], [4b], [4c], [4d], [4e], [4f])      
   
   
   
   int i, j;
   real temp;
   real change, maxChange;
   double Start2, End2, Start3, End3, Start4, End4, Start5, End5;  
   
   cout << endl << "SOLVING CYCLE STARTS...";
   cout << endl << "============================================" << endl;   
   
   for (iter = 1; iter < nonlinearIterMax; iter++) {
      Start5 = getHighResolutionTime();   
      cout << endl << "ITERATION NO = " << iter << endl;
      
      // -----------------------S T A R T   O F   S T E P  1------------------------------------
      // (1) solve SCPE for pressure correction delta(p)
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4a])   
      Start2 = getHighResolutionTime();      
      Start3 = getHighResolutionTime();     
      
      applyBC_p();
      applyBC();  
      
      End3 = getHighResolutionTime();
      printf("   Time for both applyBC's             = %-.4g seconds.\n", End3 - Start3);          
      Start3 = getHighResolutionTime();   
      
      calcGlobalSys_p();
      
      End3 = getHighResolutionTime();
      printf("   Time for calcGlobalSys for all      = %-.4g seconds.\n", End3 - Start3);   
      
      Start3 = getHighResolutionTime();         

      for (i=0; i<NN; i++) {
         K_u_diagonal[i] = 1.0/K_u_diagonal[i];
         K_v_diagonal[i] = 1.0/K_v_diagonal[i];
         K_w_diagonal[i] = 1.0/K_w_diagonal[i];
      }
      
      End3 = getHighResolutionTime();
      printf("   Time for taking inv of dia(K)       = %-.4g seconds.\n", End3 - Start3);   
      
      Start3 = getHighResolutionTime();         
            
      applyBC_p();
      applyBC();      
      End3 = getHighResolutionTime();
      printf("   Time for both applyBC's             = %-.4g seconds.\n", End3 - Start3);    
      
      Start3 = getHighResolutionTime();           
      #ifdef CG_CUDA
         CUSP_pC_CUDA_CG();
      #endif
      #ifdef CR_CUDA
         CUSP_pC_CUDA_CR();
      #endif
      #ifdef CG_CUSP
         CUSP_pC_CUSP_CG();
      #endif
      #ifdef CR_CUSP
         CUSP_pC_CUSP_CR();
      #endif      
      End3 = getHighResolutionTime();
      printf("   Time for CUSP op's + CR solver      = %-.4g seconds.\n", End3 - Start3);      
      
      End2 = getHighResolutionTime();
      printf("Total time for STEP 1         = %-.4g seconds.\n", End2 - Start2);           
      cout << "STEP 1 is okay: delta(p)^(i+1/2) is calculated." << endl;
      // delta(p)^(i+1/2) is calculated
      // -------------------------E N D   O F   S T E P  1--------------------------------------
      
      
      
      // -----------------------S T A R T   O F   S T E P  2------------------------------------    
      // (2) mass-adjust velocity field and increment pressure via [4b], [4c], [4d].
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4b], [4c], [4d]) 
      
      Start2 = getHighResolutionTime();      
      for (phase=0; phase<3; phase++) {    // Defines on which dimension(x, y, z) calculations takes place
         vectorOperationNo = 2;
         vectorProduct();   // Calculates C_x * deltaP^(i+1/2), C_y * deltaP^(i+1/2), C_z * deltaP^(i+1/2) at GPU according to phase.
                            // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4b], [4c])
                            
         switch (phase) {   // Defines on which dimension(x, y, z) calculations takes place
            case 0:
               for (i=0; i<NN; i++) {
                  u[i] += K_u_diagonal[i]*F[i];   // Calculate u^(i+1/2)
               }
               break;
            case 1:
               for (i=0; i<NN; i++) {
                  v[i] += K_v_diagonal[i]*F[i];   // Calculate v^(i+1/2)
               } 
               break;
            case 2:
               for (i=0; i<NN; i++) {
                  w[i] += K_w_diagonal[i]*F[i];   // Calculate w^(i+1/2)
               }  
               break;
         }
         applyBC();
      }   // End of phase loop
      
      for (i=0; i<NN; i++) {
         p[i] = p[i] + (1.0-alpha[3]) * delta_p[i];   // Calculate p^(i+1)
      }
      
      End2 = getHighResolutionTime();
      printf("Total time for STEP 2         = %-.4g seconds.\n", End2 - Start2);         
      cout << "STEP 2 is okay: u^(i+1/2), v^(i+1/2), w^(i+1/2) & p^(i+1) are calculated." << endl;        
      // u^(i+1/2), v^(i+1/2), w^(i+1/2) & p^(i+1) are calculated      
      // -------------------------E N D   O F   S T E P  2--------------------------------------     

      
      
      // -----------------------S T A R T   O F   S T E P  3------------------------------------ 
      // Solve x, y and z momentum equations([4e], [4f]) for u, v, w
      // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4e], [4f])       
      
      Start2 = getHighResolutionTime();
      
      for (phase=0; phase<3; phase++) {   // Defines on which dimension(x, y, z) calculations takes place
         Start4 = getHighResolutionTime(); 
         Start3 = getHighResolutionTime();  
         calcGlobalSys_mom();
         
         switch (phase) {
            case 0:
               End3 = getHighResolutionTime();                  
               printf("      Time for calcGlobalSys for x        = %-.4g seconds.\n", End3 - Start3); 
               break;
            case 1:
               End3 = getHighResolutionTime();                  
               printf("      Time for calcGlobalSys for y        = %-.4g seconds.\n", End3 - Start3); 
               break;
            case 2: 
               End3 = getHighResolutionTime();                  
               printf("      Time for calcGlobalSys for z        = %-.4g seconds.\n", End3 - Start3); 
               break;
         }
         
         Start3 = getHighResolutionTime();
         applyBC_p();         
         applyBC();
         End3 = getHighResolutionTime();
         printf("      Time for both applyBC's             = %-.4g seconds.\n", End3 - Start3);   

         Start3 = getHighResolutionTime();           
         vectorOperationNo = 1;
         vectorProduct();   // Calculates f_u + C_x * p (@GPU) or f_v + C_y * p (@GPU) or f_w + C_z * p (@GPU) according to phase.
                            // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4e], [4f])     
         End3 = getHighResolutionTime();
         switch (phase) {   // Defines on which dimension(x, y, z) calculations takes place
            case 0:                 
               printf("      Time for f_u + C_x * p (@GPU)       = %-.4g seconds.\n", End3 - Start3); 
               break;
            case 1:                 
               printf("      Time for f_v + C_y * p (@GPU)       = %-.4g seconds.\n", End3 - Start3);     
               break;
            case 2:                
               printf("      Time for f_w + C_z * p (@GPU)       = %-.4g seconds.\n", End3 - Start3);     
               break;
         }
         
         Start3 = getHighResolutionTime();
         switch (phase) {   // Defines on which dimension(x, y, z) calculations takes place
            case 0:
               for (i=0; i<NN; i++) {
                  F[i]= (alpha[0]/(1.0-alpha[0]))*tempDiagonal[i]*u[i] + F[i]; // Calculates final values for RHS of the x-momentum equation
               }                                                               // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4e])    
               End3 = getHighResolutionTime();
               printf("      Time for K_u(dia) * u + [C_x*p]     = %-.4g seconds.\n", End3 - Start3);                  
               break;
            case 1:
               for (i=0; i<NN; i++) {
                  F[i]= (alpha[1]/(1.0-alpha[1]))*tempDiagonal[i]*v[i] + F[i]; // Calculates final values for RHS of the y-momentum equation
               }                                                               // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4f])  
               End3 = getHighResolutionTime();
               printf("      Time for K_v(dia) * v + [C_y*p]     = %-.4g seconds.\n", End3 - Start3);   
               break;
            case 2:
               for (i=0; i<NN; i++) {
                  F[i]= (alpha[2]/(1.0-alpha[2]))*tempDiagonal[i]*w[i] + F[i]; // Calculates final values for RHS of the x-momentum equation
               }                                                               // (Segregated Finite Element Algorithms for the Numerical Solution of Large-Scale Incompressible Flow Problems, Vahe Horoutunian, [4f*])               
               End3 = getHighResolutionTime();                                 // *Paper derives equations for two dimensions.
               printf("      Time for K_w(dia) * w + [C_z*p]     = %-.4g seconds.\n", End3 - Start3);  
               break;
         }           

         Start3 = getHighResolutionTime();   
         applyBC();
         End3 = getHighResolutionTime();
         printf("      Time for both applyBC's             = %-.4g seconds.\n      ", End3 - Start3);   
         
         Start3 = getHighResolutionTime();     
         #ifdef GMRES_CUSP
            CUSP_GMRES(); // Non-sym, positive def
         #endif
         #ifdef BiCG_CUSP
            CUSP_BiCG();  // Non-sym, positive def 
         #endif
         End3 = getHighResolutionTime();
         printf("\n      Time for momentum eq solver         = %-.4g seconds.", End3 - Start3);     
         switch (phase) {   // Defines on which dimension(x, y, z) calculations takes place
            case 0:
               cout << endl << "   x-momentum is solved." << endl; 
               End4 = getHighResolutionTime();
               printf("   Total time for solving x-momentum   = %-.4g seconds.\n", End4 - Start4);    
               break;
            case 1:  
               cout << endl << "   y-momentum is solved." << endl; 
               End4 = getHighResolutionTime();
               printf("   Total time for solving y-momentum   = %-.4g seconds.\n", End4 - Start4);                   
               break;
            case 2: 
               cout << endl << "   z-momentum is solved." << endl;
               End4 = getHighResolutionTime();
               printf("   Total time for solving z-momentum   = %-.4g seconds.\n", End4 - Start4);                   
               break;
         }
      }   // End of phase loop
      for (i=0; i<NN; i++) {
         u[i] = u_temp[i];
         v[i] = v_temp[i];
         w[i] = w_temp[i];         
      }   
      
      End2 = getHighResolutionTime();
      printf("Total time for STEP 3         = %-.4g seconds.\n", End2 - Start2);         
      cout << "STEP 3 is okay: u^(i+1), v^(i+1), w^(i+1) are calculated." << endl;
      // Momentum equations are solved. u^(i+1), v^(i+1), w^(i+1) are calculated.
      // -------------------------E N D   O F   S T E P  3--------------------------------------
      
      
      
      // Calculates maximum error/change for checking convergence.
      Start2 = getHighResolutionTime();   
      maxChange = abs(delta_p[0]);
      
      for (i=1; i<NN; i++) {
         change = abs(delta_p[i]);
         if (change > maxChange) {
            maxChange = change;
         }
      }
      
      End2 = getHighResolutionTime();
      
      printf("Total time for calc maxChange = %-.4g seconds.\n", End2 - Start2);  
      
      End5 = getHighResolutionTime();      
      
      cout <<  " Iter |   Time(sec)   |  Max. Change  |    Mon u    |    Mon v    |    Mon w    |    Mon p  " << endl;
      cout <<  "============================================================================================" << endl;        
      printf("%5d %10.4g %19.5e", iter, End5 - Start5, maxChange);

      if (nMonitorPoints > 0) {
         printf("%11d %14.4e %13.4e %13.4e %13.4e\n", monitorNodes[0],
                                                      u[monitorNodes[0]],
                                                      u[monitorNodes[0] + NN],
                                                      u[monitorNodes[0] + NN*2],
                                                      u[monitorNodes[0] + NN*3]);
         for (i=1; i<nMonitorPoints; i++) {
            printf("%59d %14.4e %13.4e %13.4e %13.4e\n", monitorNodes[i],
                                                         u[monitorNodes[i]],
                                                         u[monitorNodes[i] + NN],
                                                         u[monitorNodes[i] + NN*2],
                                                         u[monitorNodes[i] + NN*3]);
         } 
      }
      cout << endl;
      
      if (maxChange < nonlinearTol && iter > 1) {
         for (phase=0; phase<3; phase++) {
            applyBC();
         }
         applyBC_p();
         break;
      }
      
      // Write Tecplot file
      if(iter % nDATiter == 0 || iter == nonlinearIterMax) {
         writeTecplotFile();
         // cout << "A DAT file is created for Tecplot." << endl;
      }      
        
   }   // End of nonlinearIter loop
   
   
   // Giving info about convergence
   if (iter > nonlinearIterMax) { 
      cout << endl << "Solution did not converge in " << nonlinearIterMax << " iterations." << endl; 
   }
   else {
      cout << endl << "Convergence is achieved at " << iter << " iterations." << endl; 
      writeTecplotFile();      
   }

}  // End of function solve()








//------------------------------------------------------------------------------
void readRestartFile()
//------------------------------------------------------------------------------
{
   // Reads the restart file, which is a Tecplot DAT file

   double dummy;
   ifstream restartFile;
   
   restartFile.open((whichProblem + restartExtension).c_str(), ios::in);
     
   restartFile.ignore(256, '\n');   // Read and ignore the line
   restartFile.ignore(256, '\n');   // Read and ignore the line
   restartFile.ignore(256, '\n');   // Read and ignore the line

   // Read u, v, w and p values
   for (int i = 0; i<NN; i++) {
      restartFile >> dummy >> dummy >> dummy >> u[i] >> v[i] >> w[i] >> p[i];
      restartFile.ignore(256, '\n');   // Ignore the rest of the line
   }

   restartFile.close();
   
} // End of function readRestartFile()




//------------------------------------------------------------------------------
void writeTecplotFile()
//------------------------------------------------------------------------------
{
   // Write the calculated unknowns to a Tecplot file
   double x, y, z;
   int i, e;

   ostringstream dummy;
   dummy << iter;
   outputExtension = "_" + dummy.str() + ".dat";

   datFile.open((whichProblem + outputExtension).c_str(), ios::out);
   
   datFile << "TITLE = " << whichProblem << endl;
   datFile << "VARIABLES = X,  Y,  Z,  U, V, W, P" << endl;
   
   if (eType == 1) {
      datFile << "ZONE N=" <<  NN  << " E=" << NE << " F=FEPOINT ET=QUADRILATERAL" << endl;
      }        
   else if (eType == 3) {
      datFile << "ZONE NODES=" <<  NN  << ", ELEMENTS=" << NE << ", DATAPACKING=POINT, ZONETYPE=FEBRICK" << endl;   
      }
      else { 
      datFile << "ZONE NODES=" <<  NN  << ", ELEMENTS=" << NE << ", DATAPACKING=POINT, ZONETYPE=FETETRAHEDRON" << endl;   
      }

   // Print the coordinates and the calculated values
   for (i = 0; i<NN; i++) {
      x = coord[i][0];
      y = coord[i][1];
      z = coord[i][2];  
      datFile.precision(5);
      datFile << scientific  << "\t" << x << " "  << y << " "  << z << " " << u[i] << " " << v[i] << " " << w[i] << " " << p[i] << endl;
   }

   // Print the connectivity list
   for (e = 0; e<NE; e++) {
      datFile << fixed << "\t";
      for (i = 0; i<NENv; i++) {
         // datFile.precision(5);
         datFile << LtoG[e][i]+1 << " " ;
      }
      datFile<< endl;
   }

   datFile.close();
} // End of function writeTecplotFile()


*/
