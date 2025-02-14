/***************************************************************************
 * rte3D_varOrd.cc             Surya Mohan           11/01/2011         *
 *                                                                         *
 ***************************************************************************/

#ifdef __BORLANDC__
#include <strstrea.h>
#include <conio.h>
#include <process.h>

typedef unsigned pid_t;
#else
#include <sstream>
#include <unistd.h>

#endif
#include <iostream>
#include <fstream>
#include <string.h>
#include <algorithm>
#include <vector>
#include <set>
#include <time.h>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mathlib.h>
#include "matrix.h"
#include <felib.h>
#include <pthread.h>
#include "source.h"
#include "pparse.h"
#include "rte3D_math.h"
#include "phaseFunc.h"
#define VARYDELTA

using namespace toast;

/*Data context class containing necessary objects required by GMRES implicit solver*/
class MyDataContext{
public:
CCompRowMatrix SAint_co, SAintsc_co, SAintss_co, SAintc_co;
CCompRowMatrix SAintscss_co, SAintscc_co, SAintssc_co;
CCompRowMatrix Sdxx, Sdyy, Sdzz;
CCompRowMatrix SPS, SPSdx, SPSdy, SPSdz;
RCompRowMatrix Aint, Aintsc, Aintss, Aintc;
RCompRowMatrix Aintscsc,  Aintscss, Aintscc, Aintssss,  Aintssc, Aintcc;
RCompRowMatrix apu1, apu1sc, apu1ss, apu1c;
RCompRowMatrix A2, b1;
int mfc_count;
int spatN, sphOrder, maxAngN;
IVector nodal_sphOrder, node_angN, offset;
const toast::complex *saint_coval, *saintsc_coval, *saintss_coval, *saintc_coval;
const toast::complex *saintscss_coval, *saintscc_coval, *saintssc_coval; 

const toast::complex *sdxxval, *sdyyval, *sdzzval; 
const toast::complex *spsval, *spsdxval, *spsdyval, *spsdzval; 

const double *aintval, *aintscval, *aintssval, *aintcval, *apu1val, *apu1scval, *apu1ssval, *apu1cval;
const double *aintscscval, *aintscssval, *aintssssval, *aintsccval, *aintsscval, *aintccval;
const double *a2val;

//CCompRowMatrix Xmat;
RDenseMatrix *Ylm;
//complex *xmatval;
double w, c;
int *xrowptr, *xcolidx;

void initialize(QMMesh &spatMesh, const IVector& nodal_sphOrder, RVector &delta, RVector &mua, RVector &mus, RVector &ref, const double g, toast::complex (*phaseFunc)(const double g, const double costheta), double w, double c, const RDenseMatrix& pts, const RVector &wts)
{ 
	this->w = w;
	this->c = c;
	this->nodal_sphOrder = nodal_sphOrder;
	sphOrder = vmax(nodal_sphOrder);

	/*Precomputing spherical harmonics over the quadarture points 
	which are required by the boundary integrals*/
	Ylm = new RDenseMatrix[sphOrder +1];
	for(int l=0; l<=sphOrder; l++)
		Ylm[l].New(2*l+1, pts.nRows());
	sphericalHarmonics(sphOrder, pts.nRows(), pts, Ylm);

	spatN = spatMesh.nlen();

	/*Computing the angular degrees of freedom at each spatial node*/
	node_angN.New(spatN);
	for(int i=0; i < spatN; i++)
	{
		node_angN[i] = 0;
		for(int l=0; l <= nodal_sphOrder[i]; l++)
			node_angN[i] += (2*l+1);
	}
	maxAngN = vmax(node_angN);

	/*Computes the start location for a given spatial node 
	in the system matrix or solution vector*/
	offset.New(spatN);
	offset[0] = 0;
	for(int i=1; i < spatN; i++)
		offset[i] = offset[i-1] + node_angN[i-1];

 
	/*Allocating memory for Ax where A is an angular matrix and x is the solution vector*/
	CCompRowMatrix Sint, Sdx, Sdy, Sdz;
	CCompRowMatrix Sx, Sy, Sz, Sdxy, Sdyx, Sdzx, Sdxz, Sdyz, Sdzy;
	CCompRowMatrix spatA3_rte, spatA3_sdmx, spatA3_sdmy, spatA3_sdmz;	
	cout<<"Generating spatial integrals ..."<<endl;
	gen_spatint_3D(spatMesh, mua, mus, ref, delta, w, c, Sint, Sdx, Sdy, Sdz, Sx, Sy, Sz, Sdxx, Sdxy, Sdyx, Sdyy, Sdxz, Sdzx, Sdyz, Sdzy, Sdzz, spatA3_rte, spatA3_sdmx, spatA3_sdmy, spatA3_sdmz, SPS, SPSdx, SPSdy, SPSdz);
	Sint = Sint*toast::complex(w/c, 0); Sdx = Sdx*toast::complex(w/c, 0); Sdy = Sdy*toast::complex(w/c, 0); Sdz = Sdz*toast::complex(w/c, 0);
	SAint_co = Sint + spatA3_rte; SAintsc_co = Sdx + spatA3_sdmx - Sx; SAintss_co = Sdy + spatA3_sdmy - Sy;
	SAintc_co = Sdz + spatA3_sdmz - Sz; SAintscss_co = Sdxy + Sdyx; SAintscc_co = Sdxz + Sdzx; SAintssc_co = Sdyz + Sdzy;
	
	/*Dereferencing the val pointers of spatial matrices*/
	saint_coval = SAint_co.ValPtr(); saintsc_coval = SAintsc_co.ValPtr(); saintss_coval = SAintss_co.ValPtr();
	saintc_coval = SAintc_co.ValPtr(); saintscss_coval = SAintscss_co.ValPtr(); saintscc_coval = SAintscc_co.ValPtr(); 
	saintssc_coval = SAintssc_co.ValPtr(); sdxxval = Sdxx.ValPtr(); sdyyval = Sdyy.ValPtr(); sdzzval = Sdzz.ValPtr(); 
	spsval = SPS.ValPtr(); spsdxval = SPSdx.ValPtr(); spsdyval = SPSdy.ValPtr(); spsdzval = SPSdz.ValPtr(); 
	
	int angN = vmax(node_angN);
	cout<<"Generating angular integrals ..."<<endl;
      	genmat_angint_3D(sphOrder, angN, Aint, Aintsc, Aintss, Aintc, Aintscsc, Aintscss, Aintscc,  Aintssss, Aintssc, Aintcc);
	
	cout<<"Generating phase integrals ..."<<endl;	
	genmat_apu(phaseFunc, g, angN, sphOrder, apu1, apu1sc, apu1ss, apu1c);

	//Writing Sint Aint and apu1 for Jacobian Computation

	cout<<"Generating boundary integrals ..."<<endl;//slow process
	genmat_boundint_3D(spatMesh, nodal_sphOrder, node_angN, offset, pts, wts, Ylm, A2, b1);
		

	/*Preparing angular integrals for computing Kronecker products implicitly*/	
        apu1.Transpone(); apu1sc.Transpone(); apu1ss.Transpone(); apu1c.Transpone(); 	
	Aint.Transpone(); Aintsc.Transpone(); Aintss.Transpone(); Aintc.Transpone();   
	Aintscsc.Transpone(); Aintscss.Transpone(); Aintscc.Transpone(); Aintssc.Transpone(); 
	Aintcc.Transpone(); Aintssss.Transpone();

        /*Dereferencing the val pointers of angular matrices*/ 

	/*Allocating memory for A2x where A2 is the matrix corresponding to boundary*/
	aintval = Aint.ValPtr(); aintscval = Aintsc.ValPtr(); aintssval = Aintss.ValPtr(); aintcval = Aintc.ValPtr();
	apu1val = apu1.ValPtr(); apu1scval = apu1sc.ValPtr(); apu1ssval = apu1ss.ValPtr(); apu1cval = apu1c.ValPtr();
	aintscscval = Aintscsc.ValPtr(); aintscssval = Aintscss.ValPtr(); aintssssval = Aintssss.ValPtr(); aintsccval = Aintscc.ValPtr();
	aintsscval = Aintssc.ValPtr(); aintccval = Aintcc.ValPtr();

	a2val = A2.ValPtr();


        /*The solution vector is stored in a matrix form for which memory is being allocated*/
	//
	xrowptr = new int[spatN + 1];
	
	xrowptr[0]=0;
	for(int i=1;i < spatN+1; i++)
		xrowptr[i] = xrowptr[i-1] + node_angN[i-1];
	
	xcolidx = new int[xrowptr[spatN]];
	int k=0;
	for(int i = 0; i < spatN; i++)
	{
		for(int j=0; j < node_angN[i]; j++)
		{
			xcolidx[k] = j;
			k++;
		}
	 }


	//Xmat.Initialise(xrowptr, xcolidx);
	//xmatval = Xmat.ValPtr();
	
	//delete []xrowptr;
	//delete []xcolidx;
}

~MyDataContext()
{
  delete []Ylm;
  delete []xrowptr;
  delete []xcolidx;

};

void WriteSparseMatrix(RCompRowMatrix &mat, char *fname, const int nr)
{
	FILE *fid;
	const int *rowptr, *colidx;
	int nzero;
		
	fid = fopen(fname, "w");
	double *valptr = mat.ValPtr();
	nzero = mat.GetSparseStructure(&rowptr, &colidx);
	for(int i=0; i < nr; i++)
	  for(int j=rowptr[i]; j< rowptr[i+1]; j++)
	 	fprintf(fid, "%d %d %f\n", i, colidx[j], valptr[j]);
	fclose(fid);
	
}
/*Generating all the spatial matrices required by variable order PN method*/
void gen_spatint_3D(const QMMesh& mesh, const RVector& muabs, const RVector& muscat, const RVector& ref, const RVector& delta, double w, double c, CCompRowMatrix& Sint, CCompRowMatrix& Sdx, CCompRowMatrix& Sdy, CCompRowMatrix& Sdz, CCompRowMatrix& Sx, CCompRowMatrix& Sy, CCompRowMatrix& Sz, CCompRowMatrix& Sdxx, CCompRowMatrix& Sdxy, CCompRowMatrix& Sdyx, CCompRowMatrix& Sdyy, CCompRowMatrix& Sdxz, CCompRowMatrix& Sdzx, CCompRowMatrix& Sdyz, CCompRowMatrix& Sdzy, CCompRowMatrix& Sdzz, CCompRowMatrix& spatA3_rte, CCompRowMatrix& spatA3_sdmx, CCompRowMatrix& spatA3_sdmy, CCompRowMatrix& spatA3_sdmz, CCompRowMatrix& SPS, CCompRowMatrix& SPSdx, CCompRowMatrix& SPSdy, CCompRowMatrix& SPSdz)
{
   int sysdim = mesh.nlen();       // dimensions are size of nodes.

   int *rowptr, *colidx, nzero;
   mesh.SparseRowStructure (rowptr, colidx, nzero);
   
   Sint.New (sysdim, sysdim);
   Sint.Initialise (rowptr, colidx);
   Sx.New (sysdim, sysdim);
   Sx.Initialise (rowptr, colidx);
   Sy.New (sysdim, sysdim);
   Sy.Initialise (rowptr, colidx);
   Sz.New (sysdim, sysdim);
   Sz.Initialise (rowptr, colidx);
   SPS.New (sysdim, sysdim);
   SPS.Initialise (rowptr, colidx); 
   spatA3_rte.New (sysdim, sysdim);
   spatA3_rte.Initialise (rowptr, colidx);
   Sdx.New (sysdim, sysdim);
   Sdx.Initialise (rowptr, colidx);
   Sdy.New (sysdim, sysdim);
   Sdy.Initialise (rowptr, colidx);
   Sdz.New (sysdim, sysdim);
   Sdz.Initialise (rowptr, colidx);
   Sdxx.New (sysdim, sysdim);
   Sdxx.Initialise (rowptr, colidx);
   Sdxy.New(sysdim, sysdim);
   Sdxy.Initialise (rowptr, colidx);
   Sdyx.New (sysdim, sysdim);
   Sdyx.Initialise (rowptr, colidx);
   Sdyy.New (sysdim, sysdim);
   Sdyy.Initialise (rowptr, colidx);
   Sdxz.New (sysdim, sysdim);
   Sdxz.Initialise (rowptr, colidx);
   Sdzx.New(sysdim, sysdim);
   Sdzx.Initialise(rowptr, colidx);
   Sdyz.New (sysdim, sysdim);
   Sdyz.Initialise (rowptr, colidx);
   Sdzy.New(sysdim, sysdim);
   Sdzy.Initialise(rowptr, colidx);
   Sdzz.New (sysdim, sysdim);
   Sdzz.Initialise (rowptr, colidx);
   SPSdx.New (sysdim, sysdim);
   SPSdx.Initialise (rowptr, colidx); 
   spatA3_sdmx.New (sysdim, sysdim);
   spatA3_sdmx.Initialise (rowptr, colidx);
   SPSdy.New (sysdim, sysdim);
   SPSdy.Initialise (rowptr, colidx); 
   spatA3_sdmy.New (sysdim, sysdim);
   spatA3_sdmy.Initialise (rowptr, colidx);
   SPSdz.New (sysdim, sysdim);
   SPSdz.Initialise (rowptr, colidx); 
   spatA3_sdmz.New (sysdim, sysdim);
   spatA3_sdmz.Initialise (rowptr, colidx);

   
   double elk_ij, elb_ij, elsx_ij, elsy_ij, elsz_ij;
   int el, nodel, i, j, k,is, js;


   
   for (el = 0; el < mesh.elen(); el++) {
	nodel = mesh.elist[el]->nNode();
	
	double dss = delta[el]; //streamline diffusion value for this element.
	double sigmatot = muabs[el] + muscat[el];
	
	RSymMatrix eldd = mesh.elist[el]->Intdd(); // "all at once!""
	for (i = 0; i < nodel; i++) {
	    if ((is = mesh.elist[el]->Node[i]) >= sysdim) continue;
	    for (j = 0; j < nodel; j++) {
		if ((js = mesh.elist[el]->Node[j]) >= sysdim) continue;
		
		elb_ij = mesh.elist[el]->IntFF (i, j);
		Sint(is,js) += toast::complex(0, elb_ij); 
		SPS(is, js) += toast::complex(elb_ij*muscat[el], 0);
		spatA3_rte(is, js) += toast::complex(elb_ij*sigmatot, 0);


		elsx_ij = mesh.elist[el]->IntFd (j,i,0);
		Sx(is,js) += toast::complex(elsx_ij, 0);
		Sdx(is,js) += toast::complex(0, dss*elsx_ij);
		SPSdx(is, js) += toast::complex(dss*elsx_ij*muscat[el], 0);
		spatA3_sdmx(is, js) += toast::complex(dss*elsx_ij*sigmatot, 0);

		elsy_ij = mesh.elist[el]->IntFd (j,i,1);
		Sy(is,js) += toast::complex(elsy_ij, 0);
		Sdy(is,js) += toast::complex(0, dss*elsy_ij);
		SPSdy(is, js) += toast::complex(dss*elsy_ij*muscat[el], 0);
		spatA3_sdmy(is, js) += toast::complex(dss*elsy_ij*sigmatot, 0);
		
		if(mesh.elist[el]->Dimension() == 3)
		{
			elsz_ij = mesh.elist[el]->IntFd (j,i,2);
			Sz(is,js) += toast::complex(elsz_ij, 0);
  			Sdz(is,js) += toast::complex(0, dss*elsz_ij);
			SPSdz(is, js) += toast::complex(dss*elsz_ij*muscat[el], 0);
			spatA3_sdmz(is, js) += toast::complex(dss*elsz_ij*sigmatot, 0);
		}
		int dim = mesh.elist[el]->Dimension();
		Sdxx(is,js) += dss * eldd(i*dim,j*dim);
	       	Sdxy(is,js) += dss * eldd(i*dim,j*dim+1);
     		Sdyx(is,js) += dss * eldd(i*dim+1,j*dim);
       		Sdyy(is,js) += dss * eldd(i*dim+1,j*dim+1);
		if(mesh.elist[el]->Dimension() == 3)
		{
	       		Sdxz(is,js) += dss * eldd(i*dim,j*dim+2);
     			Sdzx(is,js) += dss * eldd(i*dim+2,j*dim);
	       		Sdyz(is,js) += dss * eldd(i*dim+1,j*dim+2);
     			Sdzy(is,js) += dss * eldd(i*dim+2,j*dim+1);
       			Sdzz(is,js) += dss * eldd(i*dim+2,j*dim+2);	
		}
	    }
	}
   }

  delete []rowptr;
  delete []colidx;
}
/**
Computes all the angular integrals required by variable order PN approximation
**/
void genmat_angint_3D(const int sphOrder, const int angN, RCompRowMatrix& Aint, RCompRowMatrix& Aintsc, RCompRowMatrix& Aintss, RCompRowMatrix& Aintc, RCompRowMatrix& Aintscsc, RCompRowMatrix& Aintscss, RCompRowMatrix& Aintscc, RCompRowMatrix& Aintssss, RCompRowMatrix& Aintssc, RCompRowMatrix& Aintcc)
{
	RDenseMatrix dnsAint(angN, angN), dnsAintsc(angN, angN), dnsAintss(angN, angN), dnsAintc(angN, angN);
	RDenseMatrix dnsAintscsc(angN, angN), dnsAintscss(angN, angN), dnsAintcc(angN, angN);
	RDenseMatrix dnsAintscc(angN, angN), dnsAintssss(angN, angN), dnsAintssc(angN, angN);

	CVector a1(2), b1(2), c1(2), d1(2), e1(2), f1(2), p1(2), a2(2), b2(2), c2(2), d2(2), e2(2), f2(2), p2(2);
        IDenseMatrix a1c(2, 2), b1c(2, 2), c1c(2, 2), d1c(2, 2), e1c(2, 2), f1c(2, 2), p1c(2, 2);
	IDenseMatrix a2c(2, 2), b2c(2, 2), c2c(2, 2), d2c(2, 2), e2c(2, 2), f2c(2, 2), p2c(2, 2);
	 

	int is, js, indl1, indl2;

	for(int l1 = 0; l1 <= sphOrder; l1++){
		indl1 = l1*l1;
		for(int m1 = -1*l1; m1 <= l1; m1++){
			is = indl1 + l1 + m1;    	
			for(int l2 = 0; l2 <= sphOrder; l2++){
				indl2 = l2*l2;
				for(int m2 = -1*l2; m2 <= l2; m2++){
					
					js = indl2 + l2 + m2;
				
					sphY(l1, m1, p1, p1c);
					sphY(l2, m2, p2, p2c);
					dnsAint(is, js) = Integrate2(p1, p2, p1c, p2c);

					sincosY(l2, m2, a2, b2, c2, d2, a2c, b2c, c2c, d2c);
					dnsAintsc(is, js) = Integrate2(p1, a2, p1c, a2c) + Integrate2(p1, b2, p1c, b2c);
					dnsAintsc(is, js) += Integrate2(p1, c2, p1c, c2c) + Integrate2(p1, d2, p1c, d2c);

					sinsinY(l2, m2, a2, b2, c2, d2, a2c, b2c, c2c, d2c);
					dnsAintss(is, js) = Integrate2(p1, a2, p1c, a2c) + Integrate2(p1, b2, p1c, b2c);
					dnsAintss(is, js) += Integrate2(p1, c2, p1c, c2c) + Integrate2(p1, d2, p1c, d2c);

					cosY(l2, m2, e2, f2, e2c, f2c);
					dnsAintc(is, js) = Integrate2(p1, e2, p1c, e2c) + Integrate2(p1, f2, p1c, f2c);

					sincosY(l1, m1, a1, b1, c1, d1, a1c, b1c, c1c, d1c);
					sincosY(l2, m2, a2, b2, c2, d2, a2c, b2c, c2c, d2c);
					dnsAintscsc(is, js) = Integrate2(a1, a2, a1c, a2c) + Integrate2(a1, b2, a1c, b2c);
					dnsAintscsc(is, js) += Integrate2(a1, c2, a1c, c2c) + Integrate2(a1, d2, a1c, d2c);
					dnsAintscsc(is, js) += Integrate2(b1, a2, b1c, a2c) + Integrate2(b1, b2, b1c, b2c);
					dnsAintscsc(is, js) += Integrate2(b1, c2, b1c, c2c) + Integrate2(b1, d2, b1c, d2c);
					dnsAintscsc(is, js) += Integrate2(c1, a2, c1c, a2c) + Integrate2(c1, b2, c1c, b2c);
					dnsAintscsc(is, js) += Integrate2(c1, c2, c1c, c2c) + Integrate2(c1, d2, c1c, d2c);
					dnsAintscsc(is, js) += Integrate2(d1, a2, d1c, a2c) + Integrate2(d1, b2, d1c, b2c);
					dnsAintscsc(is, js) += Integrate2(d1, c2, d1c, c2c) + Integrate2(d1, d2, d1c, d2c);

					
					sinsinY(l2, m2, a2, b2, c2, d2, a2c, b2c, c2c, d2c);
					dnsAintscss(is, js) = Integrate2(a1, a2, a1c, a2c) + Integrate2(a1, b2, a1c, b2c);
					dnsAintscss(is, js) += Integrate2(a1, c2, a1c, c2c) + Integrate2(a1, d2, a1c, d2c);
					dnsAintscss(is, js) += Integrate2(b1, a2, b1c, a2c) + Integrate2(b1, b2, b1c, b2c);
					dnsAintscss(is, js) += Integrate2(b1, c2, b1c, c2c) + Integrate2(b1, d2, b1c, d2c);
					dnsAintscss(is, js) += Integrate2(c1, a2, c1c, a2c) + Integrate2(c1, b2, c1c, b2c);
					dnsAintscss(is, js) += Integrate2(c1, c2, c1c, c2c) + Integrate2(c1, d2, c1c, d2c);
					dnsAintscss(is, js) += Integrate2(d1, a2, d1c, a2c) + Integrate2(d1, b2, d1c, b2c);
					dnsAintscss(is, js) += Integrate2(d1, c2, d1c, c2c) + Integrate2(d1, d2, d1c, d2c);

					cosY(l2, m2, e2, f2, e2c, f2c);
					dnsAintscc(is, js) = Integrate2(a1, e2, a1c, e2c) + Integrate2(a1, f2, a1c, f2c);
					dnsAintscc(is, js) += Integrate2(b1, e2, b1c, e2c) + Integrate2(b1, f2, b1c, f2c);
					dnsAintscc(is, js) += Integrate2(c1, e2, c1c, e2c) + Integrate2(c1, f2, c1c, f2c);
					dnsAintscc(is, js) += Integrate2(d1, e2, d1c, e2c) + Integrate2(d1, f2, d1c, f2c);

					cosY(l1, m1, e1, f1, e1c, f1c);
					dnsAintcc(is, js) = Integrate2(e1, e2, e1c, e2c) + Integrate2(e1, f2, e1c, f2c);
					dnsAintcc(is, js) += Integrate2(f1, e2, f1c, e2c) + Integrate2(f1, f2, f1c, f2c);


					sinsinY(l1, m1, a1, b1, c1, d1, a1c, b1c, c1c, d1c);
					dnsAintssc(is, js) = Integrate2(a1, e2, a1c, e2c) + Integrate2(a1, f2, a1c, f2c);
					dnsAintssc(is, js) += Integrate2(b1, e2, b1c, e2c) + Integrate2(b1, f2, b1c, f2c);
					dnsAintssc(is, js) += Integrate2(c1, e2, c1c, e2c) + Integrate2(c1, f2, c1c, f2c);
					dnsAintssc(is, js) += Integrate2(d1, e2, d1c, e2c) + Integrate2(d1, f2, d1c, f2c);

					sinsinY(l2, m2, a2, b2, c2, d2, a2c, b2c, c2c, d2c);
					dnsAintssss(is, js) = Integrate2(a1, a2, a1c, a2c) + Integrate2(a1, b2, a1c, b2c);
					dnsAintssss(is, js) += Integrate2(a1, c2, a1c, c2c) + Integrate2(a1, d2, a1c, d2c);
					dnsAintssss(is, js) += Integrate2(b1, a2, b1c, a2c) + Integrate2(b1, b2, b1c, b2c);
					dnsAintssss(is, js) += Integrate2(b1, c2, b1c, c2c) + Integrate2(b1, d2, b1c, d2c);
					dnsAintssss(is, js) += Integrate2(c1, a2, c1c, a2c) + Integrate2(c1, b2, c1c, b2c);
					dnsAintssss(is, js) += Integrate2(c1, c2, c1c, c2c) + Integrate2(c1, d2, c1c, d2c);
					dnsAintssss(is, js) += Integrate2(d1, a2, d1c, a2c) + Integrate2(d1, b2, d1c, b2c);
					dnsAintssss(is, js) += Integrate2(d1, c2, d1c, c2c) + Integrate2(d1, d2, d1c, d2c);
					}
			}
		}
	}
	Aint = shrink(dnsAint);Aintsc = shrink(dnsAintsc); Aintss = shrink(dnsAintss);Aintc = shrink(dnsAintc);
	Aintscsc = shrink(dnsAintscsc);Aintscss = shrink(dnsAintscss);Aintscc = shrink(dnsAintscc); Aintssc = shrink(dnsAintssc);
	Aintssss = shrink(dnsAintssss);Aintcc = shrink(dnsAintcc);
}
/**
Phase function discretization
NOTE!! Only Henyey-Greenstein phase function has been implemented currently
**/
RVector phaseFuncDisc(int sphOrder, toast::complex (*phaseFunc)(const double g, const double costheta), const double g)
{
 RVector phaseFn(sphOrder+1);
 for(int l=0; l <= sphOrder; l++)
		phaseFn[l] = pow(g, (double)l);
 return(phaseFn);	
}

/** Phase integrals 
**/
void genmat_apu(toast::complex (*phaseFunc)(const double g, const double costheta), const double g, const int angN, const int sphOrder, RCompRowMatrix& apu1, RCompRowMatrix& apu1sc, RCompRowMatrix& apu1ss, RCompRowMatrix& apu1c)
{
 RDenseMatrix dnsapu1(angN, angN), dnsapu1sc(angN, angN), dnsapu1ss(angN, angN), dnsapu1c(angN, angN);
     RVector phaseFn;
    phaseFn = phaseFuncDisc(sphOrder, phaseFunc, g);
    cout<<"****************** Discretized phase function ****************"<<endl;
    cout<<phaseFn<<endl;
    cout<<"**************************************************************"<<endl;
    int indl, indm;
    int indl1, indm1, indl2, indm2, is, js;
    CVector a1(2), b1(2), c1(2), d1(2), e1(2), f1(2), p1(2), p2(2), p(2);
    IDenseMatrix a1c(2, 2), b1c(2, 2), c1c(2, 2), d1c(2, 2), e1c(2, 2), f1c(2, 2), p1c(2, 2), p2c(2, 2), pc(2, 2);
 
    for(int l1=0; l1 <= sphOrder; l1++){
    	indl1 =  l1*l1;
	for(int m1 = -1*l1; m1 <= l1; m1++){
		indm1 = l1 + m1;
		is = indl1 + indm1;
	        for(int l2=0; l2<= sphOrder; l2++){
			indl2 = l2*l2;
			for(int m2 = -1*l2; m2<=l2; m2++){
				indm2 = l2 + m2;
				js = indl2 + indm2;
				
				sphY(l1, m1, p1, p1c);
				sphY(l2, m2, p2, p2c);
				dnsapu1(is, js) += phaseFn[l2]*Integrate2(p2, p1, p2c, p1c);
				
				sincosY(l1, m1, a1, b1, c1, d1, a1c, b1c, c1c, d1c);
				dnsapu1sc(is, js) += phaseFn[l2]*(Integrate2(a1, p2, a1c, p2c) + Integrate2(b1, p2, b1c, p2c) + Integrate2(c1, p2, c1c, p2c) + Integrate2(d1, p2, d1c, p2c));
				
				sinsinY(l1, m1, a1, b1, c1, d1, a1c, b1c, c1c, d1c);
				dnsapu1ss(is, js) += phaseFn[l2]*(Integrate2(a1, p2, a1c, p2c) + Integrate2(b1, p2, b1c, p2c) + Integrate2(c1, p2, c1c, p2c) + Integrate2(d1, p2, d1c, p2c));

				cosY(l1, m1, e1, f1, e1c, f1c);
				dnsapu1c(is, js) += phaseFn[l2]*(Integrate2(e1, p2, e1c, p2c) + Integrate2(f1, p2, f1c, p2c));

			
			}
		}
	}
   }

  apu1 = shrink(dnsapu1); apu1sc = shrink(dnsapu1sc); apu1ss = shrink(dnsapu1ss);
  apu1c = shrink(dnsapu1c);
}

/** Preallocating memory for boundary integral terms.
The matrix is considered to be spatially sparse and dense angularly(since the angular sparsity pattern is not known)
which is eventually shrunk after computation.
**/
void initialiseA2b1(const Mesh &mesh, const IVector &node_angN, const IVector &offset, RCompRowMatrix& A2, RCompRowMatrix& b1)
{
   int el, nodel, i, j, k,is, js;
   int *crrowptr, *crcolidx, nzero;
   int sysdim = mesh.nlen();
   mesh.SparseRowStructure(crrowptr, crcolidx, nzero);
   
   int *status = new int[crrowptr[sysdim]];
   for(i=0; i<nzero; i++)
	status[i] = 0; // 1 implies nonzero 0 denotes zero;

   /*Computing the spatial sparsity pattern corresponding to the boundary*/
   for (el = 0; el < mesh.elen(); el++) {
        if(!mesh.elist[el]->HasBoundarySide ()) continue;
	nodel = mesh.elist[el]->nNode();
	// now determine the element integrals
	for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++)  {
	  // if sd is not a boundary side. skip 
	  if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
	  for (int i = 0; i < nodel; i++) {
	    if ((is = mesh.elist[el]->Node[i]) >= sysdim) continue;
	    for (int j = 0; j < nodel; j++) {
		if ((js = mesh.elist[el]->Node[j]) >= sysdim) continue;
		for (int rp = crrowptr[is]; rp < crrowptr[is+1]; rp++){
        		if (crcolidx[rp] == js) status[rp] = 1;
	    }
	  } 
	 }
	}
    }
   
   /*Computing the new spatial sparsity pattern corresponding to the boundary based on 'status' flag*/ 
    int tnzero=0;
    for(i=0; i < nzero; i++)
	if(status[i]) tnzero++; 
   int *spatrowptr, *spatcolidx;
   spatrowptr = new int[sysdim + 1];
   spatcolidx = new int[tnzero];
   spatrowptr[0] = 0;
   j=0;
   for(i = 0; i < sysdim; i++)
   {
	int rp1 = crrowptr[i];
	int rp2 = crrowptr[i+1];
        k=0;
	for(int rp = rp1; rp < rp2; rp++)
	{
		if(status[rp]){ 
			k++;
			spatcolidx[j] = crcolidx[rp];
			j++;			
		} 
	}
	spatrowptr[i+1] = spatrowptr[i] + k;
   } 
   delete []status;

    /*Computing the overall sparsity pattern where angular degrees of freedom are considered dense*/
    int ia, ib, ka, kb, ja, jb, idx;
    int va_i, vb_i, v_i;
    int na = sysdim, ma = sysdim, va = spatrowptr[sysdim], col;
    int *rowptr = new int[sum(node_angN)+1];
    for(i = 0; i < sum(node_angN) + 1; i++)
    	rowptr[i] = 0;
    
    k=1; 
    for (ia = 0; ia < na; ia++)
    {
	for(j = 0; j < node_angN[ia]; j++)
	{
		for(i = spatrowptr[ia]; i < spatrowptr[ia+1]; i++)
		{
			col = spatcolidx[i];
			rowptr[k] += node_angN[col];
		 }

		k++;
        } 
    }
  
    for(i = 1; i < sum(node_angN)+1; i++)
	rowptr[i] += rowptr[i-1];
	
    
   int *colidx = new int[rowptr[sum(node_angN)]];
   int col_offset;
   k=0;
   for(ia = 0; ia < na; ia++)
   {
	for(j = 0; j < node_angN[ia]; j++)
	{	
		for(i = spatrowptr[ia]; i < spatrowptr[ia+1]; i++)
		{
			col = spatcolidx[i];

			col_offset = offset[col];

			/*if(ia == 1669)
				cout<<col<< "  "<<node_angN[col]<<"  "<<node_angN[1669]<<endl;*/

			for(int l = 0; l < node_angN[col]; l++)
			{
				colidx[k] = col_offset + l;
				k++;
			}
		}
	}
   }
   A2.Initialise(rowptr, colidx);
   b1.Initialise(rowptr, colidx);
   A2.Zero(); 
   b1.Zero();

   delete []rowptr;
   delete []colidx;
   delete []spatrowptr;
   delete []spatcolidx;

}

/**Compute the boundary integral terms using quadrature
**/
void genmat_boundint_3D(const Mesh& mesh,  const IVector& sphOrder, const IVector& node_angN, const IVector& offset, const RDenseMatrix& ptsPlus, const RVector& wtsPlus, RDenseMatrix* &Ylm, RCompRowMatrix& A2, RCompRowMatrix& b1)
{
  
   const int sysdim = mesh.nlen();       // dimensions are size of nodes.
   const int fullsysdim = sum(node_angN);     // full size of angles X space nodes
   
   A2.New(fullsysdim, fullsysdim);
   b1.New(fullsysdim, fullsysdim);
   
   initialiseA2b1(mesh, node_angN, offset, A2, b1);
      
   double ela_ij;
   int el, nodel, is, js;
   for (el = 0; el < mesh.elen(); el++) {
	if(!(el*100%mesh.elen()))
		cout<<el*100/mesh.elen() <<"% progress"<<endl;
        if(!mesh.elist[el]->HasBoundarySide ()) continue;

	nodel = mesh.elist[el]->nNode();
       
	for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++)  {
	  if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
            
	  RVector nhat(3);
	  RVector temp = mesh.ElDirectionCosine(el,sd);
	  if(mesh.Dimension() == 3){
		 nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  }
	  else
	  {
		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  }
	  
	  RCompRowMatrix  Angbintplus, Angbintminus;
	  int lmaxAngN, lmaxSphOrder;
          findMaxLocalSphOrder(mesh, sphOrder, node_angN, el, lmaxSphOrder, lmaxAngN);
	  BIntUnitSphere(lmaxAngN, lmaxAngN, lmaxSphOrder, lmaxSphOrder, ptsPlus, wtsPlus, nhat, Ylm, Angbintplus, Angbintminus);

	  for (int i = 0; i < nodel; i++) {
	    if ((is = mesh.elist[el]->Node[i]) >= sysdim) continue;
	    for (int j = 0; j < nodel; j++) {
		if ((js = mesh.elist[el]->Node[j]) >= sysdim) continue;
		ela_ij = mesh.elist[el]->BndIntFFSide (i, j,sd);
		kronplus(is, js, node_angN, offset, ela_ij, Angbintplus, A2);
	 	kronplus(is, js, node_angN, offset, ela_ij, Angbintminus, b1);	
	    }
	  } 
	
        } // end loop on sides
   	
    
   } // end loop on elements

   //Shrinking the matrices 
   A2.Shrink();
   b1.Shrink();
}

};// end MyDataContext class

// =========================================================================
// global parameters
int NUM_THREADS = 8;
pthread_mutex_t sid_mutex = PTHREAD_MUTEX_INITIALIZER;
CCompRowMatrix *Xmat;
CVector *result;
CDenseMatrix *Aintx, *Aintscx, *Aintssx, *Aintcx;
CDenseMatrix *apu1x, *apu1scx, *apu1ssx, *apu1cx;
CDenseMatrix *Aintscscx, *Aintscssx, *Aintssssx, *Aintsccx, *Aintsscx, *Aintccx;
MyDataContext ctxt;
int ns;
CPrecon_Diag * AACP;
CVector *RHS, *Phi;
int sources_processed = -1;
double tol = 1e-09; 
SourceMode srctp = SRCMODE_NEUMANN;   // source type
ParamParser pp;
QMMesh qmmesh;
NodeList &nlist=qmmesh.nlist;
ElementList &elist=qmmesh.elist;
inline CVector matrixFreeCaller(const CVector& x, void * context);
void SelectSourceProfile (int &qtype, double &qwidth, SourceMode &srctp);
void SelectMeasurementProfile (ParamParser &pp, int &mtype, double &mwidth);
void genmat_source_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Source, const Mesh& mesh,  const int Nsource,const int ns, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, const RCompRowMatrix& b1, RDenseMatrix* &Ylm);
void genmat_sourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Svec, const Mesh& mesh, const int Nsource, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void genmat_toastsourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Svec, const Mesh& mesh, const RCompRowMatrix qvec, const int iq, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void genmat_toastsource_3D(const IVector& sphOrder, RCompRowMatrix* & Source, const Mesh& mesh, const RCompRowMatrix qvec, const int ns, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, const RCompRowMatrix& b1, RDenseMatrix* &Ylm);
void genmat_detector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Detector, const Mesh& mesh,  const int* Ndetector, const int nd, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void genmat_detectorvalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Dvec, const Mesh& mesh, const int Ndetector, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void genmat_toastdetector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Detector, const Mesh& mesh, const RCompRowMatrix mvec, const int nd, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void genmat_toastdetectorvalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Dvec, const Mesh& mesh, const RCompRowMatrix mvec, const int iq, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm);
void *solveForSource(void *source_idx);

void WriteData (const RVector &data, char *fname);
void WriteDataBlock (const QMMesh &mesh, const RVector &data, char *fname);
void OpenNIM (const char *nimname, const char *meshname, int size);
void WriteNIM (const char *nimname, const RVector &img, int size, int no);
bool ReadNim (char *nimname, RVector &img);
void WritePGM (const RVector &img, const IVector &gdim, char *fname);
void WritePPM (const RVector &img, const IVector &gdim,
double *scalemin, double *scalemax, char *fname);
CVector getDiag(void * context);

/* Weights for the quadrature scheme (which is accurate till 17th order) required for boundary integrals
*/
double wts17[] = {.0038282704949371616, .0038282704949371616, .0038282704949371616, .0038282704949371616, .0038282704949371616, .0038282704949371616, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0097937375124875125, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0082117372831911110, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0095954713360709628, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0099428148911781033, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283, .0096949963616630283}; 

/* Points for the quadrature scheme (which is accurate till 17th order) required for boundary integrals
*/
double pts17[110][3] = {{0, 0, 1.0}, {0, 1.0, 0}, {1.0, 0, 0}, {0, 0, -1.0}, {0, -1.0, 0}, {-1.0, 0, 0}, 

{.57735026918962576, .57735026918962576, .57735026918962576}, {.57735026918962576, .57735026918962576, -0.57735026918962576}, {.57735026918962576, -0.57735026918962576, .57735026918962576}, {-0.57735026918962576, .57735026918962576, .57735026918962576}, {-0.57735026918962576, -0.57735026918962576, .57735026918962576}, {-0.57735026918962576, .57735026918962576, -0.57735026918962576}, {.57735026918962576, -0.57735026918962576, -0.57735026918962576}, {-0.57735026918962576, -0.57735026918962576, -0.57735026918962576}, 

{0.18511563534473617, 0.18511563534473617, 0.9651240350865941}, {0.18511563534473617, 0.18511563534473617, -0.9651240350865941}, {0.18511563534473617, -0.18511563534473617, 0.9651240350865941}, {-0.18511563534473617, 0.18511563534473617, 0.9651240350865941}, {-0.18511563534473617, -0.18511563534473617, 0.9651240350865941}, {-0.18511563534473617, 0.18511563534473617, -0.9651240350865941}, {0.18511563534473617, -0.18511563534473617, -0.9651240350865941}, {-0.18511563534473617, -0.18511563534473617, -0.9651240350865941}, 

{0.18511563534473617, 0.9651240350865941, 0.18511563534473617}, {0.18511563534473617, 0.9651240350865941, -0.18511563534473617}, {0.18511563534473617, -0.9651240350865941, 0.18511563534473617}, {-0.18511563534473617, 0.9651240350865941, 0.18511563534473617}, {-0.18511563534473617, -0.9651240350865941, 0.18511563534473617}, {-0.18511563534473617, 0.9651240350865941, -0.18511563534473617}, {0.18511563534473617, -0.9651240350865941, -0.18511563534473617}, {-0.18511563534473617, -0.9651240350865941, -0.18511563534473617}, 

{0.9651240350865941, 0.18511563534473617, 0.18511563534473617}, {0.9651240350865941, 0.18511563534473617, -0.18511563534473617}, {0.9651240350865941, -0.18511563534473617, 0.18511563534473617}, {-0.9651240350865941, 0.18511563534473617, 0.18511563534473617}, {-0.9651240350865941, -0.18511563534473617, 0.18511563534473617}, {-0.9651240350865941, 0.18511563534473617, -0.18511563534473617}, {0.9651240350865941, -0.18511563534473617, -0.18511563534473617}, {-0.9651240350865941, -0.18511563534473617, -0.18511563534473617}, 

{0.39568947305594191, 0.39568947305594191, 0.8287699812525922}, {0.39568947305594191, 0.39568947305594191, -0.8287699812525922}, {0.39568947305594191, -0.39568947305594191, 0.8287699812525922}, {-0.39568947305594191, 0.39568947305594191, 0.8287699812525922}, {-0.39568947305594191, -0.39568947305594191, 0.8287699812525922}, {-0.39568947305594191, 0.39568947305594191, -0.8287699812525922}, {0.39568947305594191, -0.39568947305594191, -0.8287699812525922}, {-0.39568947305594191, -0.39568947305594191, -0.8287699812525922}, 

{0.39568947305594191, 0.8287699812525922, 0.39568947305594191}, {0.39568947305594191, 0.8287699812525922, -0.39568947305594191}, {0.39568947305594191, -0.8287699812525922, 0.39568947305594191}, {-0.39568947305594191, 0.8287699812525922, 0.39568947305594191}, {-0.39568947305594191, -0.8287699812525922, 0.39568947305594191}, {-0.39568947305594191, 0.8287699812525922, -0.39568947305594191}, {0.39568947305594191, -0.8287699812525922, -0.39568947305594191}, {-0.39568947305594191, -0.8287699812525922, -0.39568947305594191}, 

{0.8287699812525922, 0.39568947305594191, 0.39568947305594191}, {0.8287699812525922, 0.39568947305594191, -0.39568947305594191}, {0.8287699812525922, -0.39568947305594191, 0.39568947305594191}, {-0.8287699812525922, 0.39568947305594191, 0.39568947305594191}, {-0.8287699812525922, -0.39568947305594191, 0.39568947305594191}, {-0.8287699812525922, 0.39568947305594191, -0.39568947305594191}, {0.8287699812525922, -0.39568947305594191, -0.39568947305594191}, {-0.8287699812525922, -0.39568947305594191, -0.39568947305594191}, 

{0.69042104838229218, 0.69042104838229218, 0.21595729184584883}, {0.69042104838229218, 0.69042104838229218, -0.21595729184584883}, {0.69042104838229218, -0.69042104838229218, 0.21595729184584883}, {-0.69042104838229218, 0.69042104838229218, 0.21595729184584883}, {-0.69042104838229218, -0.69042104838229218, 0.21595729184584883}, {-0.69042104838229218, 0.69042104838229218, -0.21595729184584883}, {0.69042104838229218, -0.69042104838229218, -0.21595729184584883}, {-0.69042104838229218, -0.69042104838229218, -0.21595729184584883},

{0.69042104838229218, 0.21595729184584883, 0.69042104838229218}, {0.69042104838229218, 0.21595729184584883, -0.69042104838229218}, {0.69042104838229218, -0.21595729184584883, 0.69042104838229218}, {-0.69042104838229218, 0.21595729184584883, 0.69042104838229218}, {-0.69042104838229218, -0.21595729184584883, 0.69042104838229218}, {-0.69042104838229218, 0.21595729184584883, -0.69042104838229218}, {0.69042104838229218, -0.21595729184584883, -0.69042104838229218}, {-0.69042104838229218, -0.21595729184584883, -0.69042104838229218},

{0.21595729184584883, 0.69042104838229218, 0.69042104838229218}, {0.21595729184584883, 0.69042104838229218, -0.69042104838229218}, {0.21595729184584883, -0.69042104838229218, 0.69042104838229218}, {-0.21595729184584883, 0.69042104838229218, 0.69042104838229218}, {-0.21595729184584883, -0.69042104838229218, 0.69042104838229218}, {-0.21595729184584883, 0.69042104838229218, -0.69042104838229218}, {0.21595729184584883, -0.69042104838229218, -0.69042104838229218}, {-0.21595729184584883, -0.69042104838229218, -0.69042104838229218},

{0.47836902881215020, 0, 0.8781589106040662}, {0.47836902881215020, 0, -0.8781589106040662}, {-0.47836902881215020, 0, 0.8781589106040662}, {-0.47836902881215020, 0, -0.8781589106040662}, {0.8781589106040662, 0, 0.47836902881215020}, {0.8781589106040662, 0, -0.47836902881215020}, {-0.8781589106040662, 0, 0.47836902881215020}, {-0.8781589106040662, 0, -0.47836902881215020}, 

{0, 0.8781589106040662, 0.47836902881215020}, {0, 0.8781589106040662, -0.47836902881215020}, {0, -0.8781589106040662, 0.47836902881215020}, {0, -0.8781589106040662, -0.47836902881215020}, {0, 0.47836902881215020, 0.8781589106040662}, {0, 0.47836902881215020, -0.8781589106040662}, {0, -0.47836902881215020, 0.8781589106040662}, {0, -0.47836902881215020, -0.8781589106040662}, 

{0.47836902881215020, 0.8781589106040662, 0}, {0.47836902881215020, -0.8781589106040662, 0}, {-0.47836902881215020, 0.8781589106040662, 0}, {-0.47836902881215020, -0.8781589106040662, 0}, {0.8781589106040662, 0.47836902881215020, 0}, {0.8781589106040662, -0.47836902881215020, 0}, {-0.8781589106040662, 0.47836902881215020, 0}, {-0.8781589106040662, -0.47836902881215020, 0}};

/* Computes the source vectors for all the boundary sources when a QM file has been specified
*/
void genmat_toastsource_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Source, const Mesh& mesh, const RCompRowMatrix qvec, const int ns, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, const RCompRowMatrix& b1, RDenseMatrix* &Ylm)
{
   int sysdim = mesh.nlen();       
   int fullsysdim = sum(node_angN);   
   RCompRowMatrix Svec;
   for (int i = 0; i < ns; i++) {
     Source[i].New(fullsysdim,1);
     genmat_toastsourcevalvector_3D(sphOrder, node_angN, offset, Svec, mesh, qvec,i, dirVec, is_cosine,  pts, wts, Ylm);
     b1.AB(Svec, Source[i]);
   }
}

/* Computes the source vector per a boundary source when a QM file has been specified
*/
void genmat_toastsourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Svec, const Mesh& mesh, const RCompRowMatrix qvec, const int iq, const RVector& dirVec, const bool is_cosine,  const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       
   int fullsysdim = sum(node_angN);      
   int *rowptr, *colidx;
   rowptr = new int[fullsysdim+1];
   colidx = new int[fullsysdim];
   rowptr[0] = 0;
   for(int i=1;  i <= fullsysdim; i++)
	rowptr[i] = i;
   for(int i=0; i<fullsysdim; i++)
	colidx[i] = 0;
   Svec.New (fullsysdim,1);
   Svec.Initialise(rowptr, colidx);

   int row_offset = 0;
   RDenseMatrix Ystarlm;
   RDenseMatrix dirMat(1, 3);

   for (int jq = qvec.rowptr[iq]; jq < qvec.rowptr[iq+1]; jq++) {
   	int Nsource = qvec.colidx[jq];
   	double sweight = norm(qvec.Get(iq,Nsource)); 
   	for (el = 0; el < mesh.elen(); el++) {
        	if(!mesh.elist[el]->IsNode(Nsource)) continue; // source not in this el
        	if(!mesh.elist[el]->HasBoundarySide()) continue;
		for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  		// if sd is not a boundary side. skip 
	  		if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
 			RVector nhat(3);
	  		RVector temp = mesh.ElDirectionCosine(el,sd);
	  		if(mesh.Dimension() == 3){
		 		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  		}
	  		else
	  		{
				nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  		}
			dirMat(0, 0) = dirVec[0]; dirMat(0, 1) = dirVec[1]; dirMat(0, 2) = dirVec[2]; 
	  		for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    			is = mesh.elist[el]->SideNode(sd,nd);
	    			js = mesh.elist[el]->Node[is];
				double ela_i =  mesh.elist[el]->IntF(is);
	    			if(is_cosine)
				{
					RDenseMatrix Angsvec(node_angN[js], 1);
					for(int l=0; l<= sphOrder[js]; l++){
						int indl = l*l;
						for(int m=-l; m<=l; m++){
							int indm = l + m;
							for(int i=0; i < pts.nRows(); i++)
							{
								double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
								if(sdotn<0)
									Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(-sdotn)*wts[i]*4*M_PI;
							}
						}
					}
					for(int i = 0; i < node_angN[js]; i++)
						Svec(offset[js] + i, 0) += Angsvec(i, 0)*sweight*ela_i;

				}
				else{
					RDenseMatrix Angsvec(node_angN[js], 1);
					RDenseMatrix *Ystarlm;
					Ystarlm = new RDenseMatrix[sphOrder[js]+1];
					for(int l=0; l<= sphOrder[js]; l++)
						Ystarlm[l].New(2*l+1, 1);
					sphericalHarmonics(sphOrder[js], 1, dirMat, Ystarlm);
					for(int l=0; l<= sphOrder[js]; l++){
						int indl = l*l;
						for(int m=-l; m<=l; m++){
							int indm = l + m;
							Angsvec(indl+indm, 0) = Ystarlm[l](indm, 0)*(4*M_PI)/(2*l+1);
						}
					}
					for(int i = 0; i < node_angN[js]; i++)
						Svec(offset[js] + i, 0) += Angsvec(i, 0)*sweight*ela_i;

					delete []Ystarlm;
				}
	    		}

		} // end loop on element sides

   	} // end loop on elements
  }
   delete []rowptr;
   delete []colidx;
}  

/** Computes source vectors for point sources on the boundary 
**/
void genmat_source_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Source, const Mesh& mesh,  const int* Nsource, const int ns, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, const RCompRowMatrix& b1, RDenseMatrix* &Ylm)
{
   int sysdim = mesh.nlen();         // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);       // full size of angles X space nodes

   RCompRowMatrix Svec;
   for (int i = 0; i < ns; i++) {
     Source[i].New(fullsysdim,1);
     genmat_sourcevalvector_3D(sphOrder, node_angN, offset, Svec, mesh, Nsource[i], dirVec, is_cosine, pts, wts, Ylm);
     b1.AB(Svec, Source[i]);
   }
}

/**Computes source vector for a single point source on the boundary
**/
void genmat_sourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Svec, const Mesh& mesh, const int Nsource, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);   // full size of angles X space nodes
   
   int *rowptr, *colidx;
   rowptr = new int[fullsysdim+1];
   colidx = new int[fullsysdim];
   rowptr[0] = 0;
   for(int i=1;  i <= fullsysdim; i++)
	rowptr[i] = i;
   for(int i=0; i<fullsysdim; i++)
	colidx[i] = 0;
   Svec.New (fullsysdim,1);
   Svec.Initialise(rowptr, colidx);

   int row_offset = 0;
   RDenseMatrix dirMat(1, 3);
   RDenseMatrix Ystarlm;

   for (el = 0; el < mesh.elen(); el++) {
        if(!mesh.elist[el]->IsNode(Nsource)) continue; // source not in this el
        if(!mesh.elist[el]->HasBoundarySide()) continue;
	for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  // if sd is not a boundary side. skip 
	  if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
	  
	  RVector nhat(3);
	  RVector temp = mesh.ElDirectionCosine(el,sd);
	  if(mesh.Dimension() == 3){
		 nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  }
	  else
	  {
		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  }
	  dirMat(0, 0) = dirVec[0]; dirMat(0, 1) = dirVec[1]; dirMat(0, 2) = dirVec[2]; 
	  for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    is = mesh.elist[el]->SideNode(sd,nd);
	    js = mesh.elist[el]->Node[is];
	    double ela_i =  mesh.elist[el]->IntF(is);
	    if(is_cosine)
	    {
			RDenseMatrix Angsvec(node_angN[js], 1);
			for(int l=0; l<= sphOrder[js]; l++){
				int indl = l*l;
				for(int m=-l; m<=l; m++){
					int indm = l + m;
					for(int i=0; i < pts.nRows(); i++)
					{
						double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
						if(sdotn<0)
							Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(-sdotn)*wts[i]*4*M_PI;
					}
				}
			}
			for(int i = 0; i < node_angN[js]; i++)
				Svec(offset[js] + i, 0) = Angsvec(i, 0)*ela_i;
			
		}
	    	else{
			RDenseMatrix Angsvec(node_angN[js], 1);
			RDenseMatrix *Ystarlm;
			Ystarlm = new RDenseMatrix[sphOrder[js]+1];
			for(int l=0; l<= sphOrder[js]; l++)
			 Ystarlm[l].New(2*l+1, 1);
			sphericalHarmonics(sphOrder[js], 1, dirMat, Ystarlm);
			for(int l=0; l<= sphOrder[js]; l++){
				int indl = l*l;
				for(int m=-l; m<=l; m++){
					int indm = l + m;
					Angsvec(indl+indm, 0) = Ystarlm[l](indm, 0)*(4*M_PI)/(2*l+1);
				}
			}
			for(int i = 0; i < node_angN[js]; i++)
				Svec(offset[js] + i, 0) = Angsvec(i, 0)*ela_i;
			delete []Ystarlm;
		}
	}
    } 


   } // end loop on elements
   delete []rowptr;
   delete []colidx;
}

/**Computes source vector for a single source in the interior of the domain where the source profile is defined in a QM file
**/
void genmat_toastintsourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RVector& Svec, const Mesh& mesh, const RCompRowMatrix qvec, const int iq, const RVector& dirVec, const bool is_cosine,  const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);   // full size of angles X space nodes
   
   int row_offset = 0;
   RDenseMatrix Ystarlm;
   RDenseMatrix dirMat(1, 3);

   // now create vector, by looping over elements that have a boundary
   for (int jq = qvec.rowptr[iq]; jq < qvec.rowptr[iq+1]; jq++) {
   	int Nsource = qvec.colidx[jq];
   	double sweight = norm(qvec.Get(iq,Nsource)); 
   	for (el = 0; el < mesh.elen(); el++) {
		for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  		// if sd is not a boundary side. skip 
 			RVector nhat(3);
	  		RVector temp = mesh.ElDirectionCosine(el,sd);
	  		if(mesh.Dimension() == 3){
		 		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  		}
	  		else
	  		{
				nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  		}
			dirMat(0, 0) = -1*nhat[0]; dirMat(0, 1) = -1*nhat[1]; dirMat(0, 2) = -1*nhat[2]; 
	  		for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    			is = mesh.elist[el]->SideNode(sd,nd);
	    			js = mesh.elist[el]->Node[is];
				double ela_i =  mesh.elist[el]->IntF(is);
	    			if(is_cosine)
				{
					RDenseMatrix Angsvec(node_angN[js], 1);
					for(int l=0; l<= sphOrder[js]; l++){
						int indl = l*l;
						for(int m=-l; m<=l; m++){
							int indm = l + m;
							for(int i=0; i < pts.nRows(); i++)
							{
								double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
								if(sdotn<0)
									Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(-sdotn)*wts[i]*4*M_PI;
							}
						}
					}
					for(int i = 0; i < node_angN[js]; i++)
						Svec[offset[js] + i] += Angsvec(i, 0)*sweight;

				}
				else{
					RDenseMatrix Angsvec(node_angN[js], 1);
					RDenseMatrix *Ystarlm;
					Ystarlm = new RDenseMatrix[sphOrder[js]+1];
					for(int l=0; l<= sphOrder[js]; l++)
						Ystarlm[l].New(2*l+1, 1);
					sphericalHarmonics(sphOrder[js], 1, dirMat, Ystarlm);
					for(int l=0; l<= sphOrder[js]; l++){
						int indl = l*l;
						for(int m=-l; m<=l; m++){
							int indm = l + m;
							Angsvec(indl+indm, 0) = Ystarlm[l](indm, 0)*(4*M_PI)/(2*l+1);
						}
					}
					for(int i = 0; i < node_angN[js]; i++)
						Svec[offset[js] + i] += Angsvec(i, 0)*sweight;

					delete []Ystarlm;
				}
	    		}

		} // end loop on element sides

   	} // end loop on elements
  }
}  

/**Computes source vector for a single point source in the interior of the domain
**/
void genmat_intsourcevalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RVector& Svec, const Mesh& mesh, const int Nsource, const RVector& dirVec, const bool is_cosine, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);   // full size of angles X space nodes
   
   int row_offset = 0;
   RDenseMatrix dirMat(1, 3);
   RDenseMatrix Ystarlm;

   // now create vector, by looping over elements that have a boundary
   for (el = 0; el < mesh.elen(); el++) {
        if(!mesh.elist[el]->IsNode(Nsource)) continue; // source not in this el
	for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  RVector nhat(3);
	  RVector temp = mesh.ElDirectionCosine(el,sd);
	  if(mesh.Dimension() == 3){
		 nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  }
	  else
	  {
		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  }
	  dirMat(0, 0) = dirVec[0]; dirMat(0, 1) = dirVec[1]; dirMat(0, 2) = dirVec[2]; 
	  //cout<<"direction vector: "<<dirMat<<endl;
	  for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    is = mesh.elist[el]->SideNode(sd,nd);
	    js = mesh.elist[el]->Node[is];
	  // if(js != Nsource) continue;
	    double ela_i =  mesh.elist[el]->IntF(is);
	    if(is_cosine)
	    {
			RDenseMatrix Angsvec(node_angN[js], 1);
			for(int l=0; l<= sphOrder[js]; l++){
				int indl = l*l;
				for(int m=-l; m<=l; m++){
					int indm = l + m;
					for(int i=0; i < pts.nRows(); i++)
					{
						double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
						if(sdotn<0)
							Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(-sdotn)*wts[i]*4*M_PI;
					}
				}
			}
			for(int i = 0; i < node_angN[js]; i++)
				Svec[offset[js] + i] += Angsvec(i, 0);
			
		}
	    	else{
			RDenseMatrix Angsvec(node_angN[js], 1);
			RDenseMatrix *Ystarlm;
			Ystarlm = new RDenseMatrix[sphOrder[js]+1];
			for(int l=0; l<= sphOrder[js]; l++)
			 Ystarlm[l].New(2*l+1, 1);
			sphericalHarmonics(sphOrder[js], 1, dirMat, Ystarlm);
			for(int l=0; l<= sphOrder[js]; l++){
				int indl = l*l;
				for(int m=-l; m<=l; m++){
					int indm = l + m;
					Angsvec(indl+indm, 0) = Ystarlm[l](indm, 0)*(4*M_PI)/(2*l+1);
				}
			}
			for(int i = 0; i < node_angN[js]; i++)
				Svec[offset[js] + i] += Angsvec(i, 0);
			delete []Ystarlm;

		
		}

		}
	    }



   } // end loop on elements
}
// main routine **************************************************************
/* Computes the source vectors for all the boundary sources when a QM file has been specified
*/
void genmat_toastdetector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Detector, const Mesh& mesh, const RCompRowMatrix mvec, const int nd, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int sysdim = mesh.nlen();       
   int fullsysdim = sum(node_angN);   
   //RCompRowMatrix Dvec;
   for (int i = 0; i < nd; i++) {
     Detector[i].New(fullsysdim,1);
     genmat_toastdetectorvalvector_3D(sphOrder, node_angN, offset, Detector[i], mesh, mvec,i, pts, wts, Ylm);
     //b1.AB(Dvec, Detector[i]);
   }
}

/* Computes the source vector per a boundary source when a QM file has been specified
*/
void genmat_toastdetectorvalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Dvec, const Mesh& mesh, const RCompRowMatrix mvec, const int iq, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       
   int fullsysdim = sum(node_angN);      
   int *rowptr, *colidx;
   rowptr = new int[fullsysdim+1];
   colidx = new int[fullsysdim];
   rowptr[0] = 0;
   for(int i=1;  i <= fullsysdim; i++)
	rowptr[i] = i;
   for(int i=0; i<fullsysdim; i++)
	colidx[i] = 0;
   Dvec.New (fullsysdim,1);
   Dvec.Initialise(rowptr, colidx);

   int row_offset = 0;
   RDenseMatrix Ystarlm;
   RDenseMatrix dirMat(1, 3);

   for (int jq = mvec.rowptr[iq]; jq < mvec.rowptr[iq+1]; jq++) {
   	int Ndetector = mvec.colidx[jq];
   	double sweight = norm(mvec.Get(iq,Ndetector)); 
   	for (el = 0; el < mesh.elen(); el++) {
        	if(!mesh.elist[el]->IsNode(Ndetector)) continue; // source not in this el
        	if(!mesh.elist[el]->HasBoundarySide()) continue;
		for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  		// if sd is not a boundary side. skip 
	  		if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
 			RVector nhat(3);
	  		RVector temp = mesh.ElDirectionCosine(el,sd);
	  		if(mesh.Dimension() == 3){
		 		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  		}
	  		else
	  		{
				nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  		}
	  		for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    			is = mesh.elist[el]->SideNode(sd,nd);
	    			js = mesh.elist[el]->Node[is];
				double ela_i =  mesh.elist[el]->IntF(is);
				RDenseMatrix Angsvec(node_angN[js], 1);
				for(int l=0; l<= sphOrder[js]; l++){
					int indl = l*l;
					for(int m=-l; m<=l; m++){
						int indm = l + m;
						for(int i=0; i < pts.nRows(); i++)
						{
							double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
							if(sdotn>0)
								Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(sdotn)*wts[i]*4*M_PI;
						}
					}
				}
				for(int i = 0; i < node_angN[js]; i++)
					Dvec(offset[js] + i, 0) += Angsvec(i, 0)*sweight*ela_i;

			}

		} // end loop on element sides

   	} // end loop on elements
  }
   delete []rowptr;
   delete []colidx;
}  

/** Computes source vectors for point sources on the boundary 
**/
void genmat_detector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix* & Detector, const Mesh& mesh,  const int* Ndetector, const int nd, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int sysdim = mesh.nlen();         // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);       // full size of angles X space nodes

   //RCompRowMatrix Dvec;
   for (int i = 0; i < nd; i++) {
     Detector[i].New(fullsysdim,1);
     genmat_detectorvalvector_3D(sphOrder, node_angN, offset, Detector[i], mesh, Ndetector[i], pts, wts, Ylm);
     //b1.AB(Dvec, Detector[i]);
   }
}

/**Computes source vector for a single point source on the boundary
**/
void genmat_detectorvalvector_3D(const IVector& sphOrder, const IVector& node_angN, const IVector& offset, RCompRowMatrix& Dvec, const Mesh& mesh, const int Ndetector, const RDenseMatrix& pts, const RVector& wts, RDenseMatrix* &Ylm)
{
   int el, nodel, i, j, k,is, js;
   int sysdim = mesh.nlen();       // dimensions are size of nodes.
   int fullsysdim = sum(node_angN);   // full size of angles X space nodes
   
   int *rowptr, *colidx;
   rowptr = new int[fullsysdim+1];
   colidx = new int[fullsysdim];
   rowptr[0] = 0;
   for(int i=1;  i <= fullsysdim; i++)
	rowptr[i] = i;
   for(int i=0; i<fullsysdim; i++)
	colidx[i] = 0;
   Dvec.New (fullsysdim,1);
   Dvec.Initialise(rowptr, colidx);

   int row_offset = 0;
   RDenseMatrix dirMat(1, 3);
   RDenseMatrix Ystarlm;

   for (el = 0; el < mesh.elen(); el++) {
        if(!mesh.elist[el]->IsNode(Ndetector)) continue; // source not in this el
        if(!mesh.elist[el]->HasBoundarySide()) continue;
	for(int sd = 0; sd <  mesh.elist[el]->nSide(); sd++) {
	  // if sd is not a boundary side. skip 
	  if(!mesh.elist[el]->IsBoundarySide (sd)) continue;
	  
	  RVector nhat(3);
	  RVector temp = mesh.ElDirectionCosine(el,sd);
	  if(mesh.Dimension() == 3){
		 nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = temp[2];
	  }
	  else
	  {
		nhat[0] = temp[0]; nhat[1] = temp[1]; nhat[2] = 0;
	  }
	  for(int nd = 0; nd < mesh.elist[el]->nSideNode(sd); nd++) {
	    is = mesh.elist[el]->SideNode(sd,nd);
	    js = mesh.elist[el]->Node[is];
	    double ela_i =  mesh.elist[el]->IntF(is);
	    RDenseMatrix Angsvec(node_angN[js], 1);
	    for(int l=0; l<= sphOrder[js]; l++){
		int indl = l*l;
		for(int m=-l; m<=l; m++){
			int indm = l + m;
			for(int i=0; i < pts.nRows(); i++)
			{
				double sdotn = nhat[0]*pts.Get(i, 0) + nhat[1]*pts.Get(i, 1) + nhat[2]*pts.Get(i, 2);
				if(sdotn>0)
					Angsvec(indl+indm, 0) += Ylm[l](indm, i)*(sdotn)*wts[i]*4*M_PI;
			}
		}
	    }
	   for(int i = 0; i < node_angN[js]; i++)
		Dvec(offset[js] + i, 0) = Angsvec(i, 0)*ela_i;
			
	}
    } 


   } // end loop on elements
   delete []rowptr;
   delete []colidx;
}


int main (int argc, char *argv[])
{
    char cbuf[200];
    int el;
    
    cout << "Reading mesh " << argv[1] << endl;
    ifstream ifs;
    ifs.open (argv[1]);
    xASSERT (ifs.is_open(), Mesh file not found.);
    ifs >> qmmesh;
    xASSERT (ifs.good(), Problem reading mesh.);
    ifs.close ();
    cout << "* " << qmmesh.elen() << " elements, " << qmmesh.nlen()
	 << " nodes\n";
    int dimension = nlist[0].Dim();
    for (int i = 1; i < nlist.Len(); i++)
	xASSERT(nlist[i].Dim() == dimension, Inconsistent node dimensions.);
    qmmesh.Setup();

    cout << "Forming the source\n";
    ns = 1;
    int nM=1;
    int *Nsource = new int [ns];
    int *Ndetector = new int [nM];
    int    qprof, mprof;   // source/measurement profile (0=Gaussian, 1=Cosine)
    double qwidth, mwidth; // source/measurement support radius [mm]
    RCompRowMatrix qvec, mvec;
    char file_extn[200];

    cin>>file_extn;
    cout<<"File name prefix: "<<file_extn<<endl;// prefix for the output files
    cin>>NUM_THREADS;
    cout<<"Number of threads used for this computation: "<<NUM_THREADS<<endl;
    if(argc < 3) { // point source 
         cin >> ns;
	 cout<< "Number of sources: "<<ns<<endl;
         Nsource = new int[ns];
	 cout << "source node coords: "<<endl;
	 for(int i=0; i<ns; i++){
		cin >>Nsource[i];
     	 	cout<<nlist[Nsource[i]]<<"   "<<endl;
	 }
	 cin >> nM;
	 cout<<"Number of detectors: "<<nM<<endl;
	 Ndetector = new int[nM];
	 cout << "detector node coords: "<<endl;
	 for(int i=0; i<nM; i++){
		cin>>Ndetector[i];
		cout<<nlist[Ndetector[i]]<<"  "<<endl;
	}
    }
    else { //distributed source to be read from a QM file
    cout << "QM file " << argv[2] << endl;
    ifs.open (argv[2]);
    xASSERT (ifs.is_open(), QM file not found.);
    qmmesh.LoadQM (ifs);
    xASSERT (ifs.good(), Problem reading QM.);
    ifs.close ();
    ns = qmmesh.nQ;
    nM = qmmesh.nM;
    cout << ns << " sources\n";
    SelectSourceProfile (qprof, qwidth, srctp);
    qvec.New (ns, qmmesh.nlen());
    for (int i = 0; i < ns; i++) {
	RVector q(qmmesh.nlen());
	switch (qprof) {
	case 0:
	    q = QVec_Point (qmmesh, qmmesh.Q[i], srctp);
	    break;
	case 1:
	    q = QVec_Gaussian (qmmesh, qmmesh.Q[i], qwidth, srctp);
	    break;
	case 2:
	    q = QVec_Cosine (qmmesh, qmmesh.Q[i], qwidth, srctp);
	    break;
	}
	qvec.SetRow (i, q);
    }
    cout << "Sources set "<<endl;
    SelectMeasurementProfile (pp, mprof, mwidth);
    mvec.New (nM, qmmesh.nlen());
    for (int i = 0; i < nM; i++) {
	RVector m(qmmesh.nlen());
	switch (mprof) {
	case 0:
	    m = QVec_Point (qmmesh, qmmesh.M[i], SRCMODE_NEUMANN);
	    break;
	case 1:
	    m = QVec_Gaussian (qmmesh, qmmesh.M[i], mwidth, SRCMODE_NEUMANN);
	    break;
	case 2:
	    m = QVec_Cosine (qmmesh, qmmesh.M[i], mwidth, SRCMODE_NEUMANN);
	    break;
	}
	//for (int j = 0; j < qmmesh.nlen(); j++) 
	  //m[j] *= qmmesh.plist[j].C2A();
	mvec.SetRow (i, m);
    }
    }
    //***** parameters 
    double freq = 0, g;
    int phaseExpOrder, is_cos;
    bool is_cosine;
    RVector dirVec(3);
    cin>>g;
    cout<< "value of g (WARNING !! This g is considered constant throughout the domain): "<< g<<endl;
    cin >> freq;
    cout << "value for frequency (MHz) : "<<freq<<endl;
    cin>> is_cos;
    is_cosine = is_cos>0 ? true : false;
    cout << "The source is cosine or directed (1. Cosine 0. Directed): "<<is_cosine<<endl;
    cin>> dirVec[0]; cin>> dirVec[1]; cin>>dirVec[2];
    dirVec = dirVec*1/(double)length(dirVec); // normalize the direction vector just in case
    cout<< "The direction vector for the source (if it is directed): "<<dirVec<<endl; 

    double w = freq * 2.0*M_PI*1e-6;
    double c = 0.3;
    int numpts;
    numpts = 110;
    RDenseMatrix pts(numpts, 3); 
    RVector wts(numpts);
    for(int i=0; i < numpts; i++){
	wts[i] = wts17[i]; 
	for(int j=0; j < 3; j++)
		pts(i, j) = pts17[i][j];
    }	

    RVector muscat(qmmesh.elen());
    RVector muabs(qmmesh.elen());
    RVector ref(qmmesh.elen());
    cout<< "Reading mua, mus and refractive index values for all the elements from file: "<<endl;
    for(el = 0; el <  qmmesh.elen(); el++){ // assign something to parameters
      cin >> muabs[el];
      cin >> muscat[el]; 
      cin >> ref[el]; 
    }
    
    IVector sphOrder(qmmesh.nlen());
    cout<<"Reading spherical harmonic expansion order for each node: "<<endl;
    for(int i=0; i < qmmesh.nlen(); i++)
	cin >> sphOrder[i]; 

    //smoothing parameter for the streamline diffusion modification
    RVector delta(qmmesh.elen());
    double min = 1e20, max = -1e20;
    for(el = 0; el <  qmmesh.elen(); el++){ 
#ifdef VARYDELTA 
	 double sval =  elist[el]->Size()/((muscat[el]));
	 delta[el] = sval;
#else
	 min = max = delta[0];
#endif 
    }
   ctxt.initialize(qmmesh, sphOrder, delta, muabs, muscat, ref, g, &phaseFunc, w, c, pts, wts);

     
    CVector proj(ns*nM); // projection data
    RCompRowMatrix *Source, *Detector;
    RVector b2(sum(ctxt.node_angN));
    if( !(Source = new  RCompRowMatrix [ns]))
          cerr << "Memory Allocation error Source = new  RCompRowMatrix\n";
     if( !(Detector = new  RCompRowMatrix [nM]))
          cerr << "Memory Allocation error Detector = new  RCompRowMatrix\n";

    cout<<"Computing the source and detector vectors ..."<<endl;
   if(argc<3)	
     {
      genmat_source_3D(ctxt.nodal_sphOrder, ctxt.node_angN, ctxt.offset, Source, qmmesh, Nsource, ns, dirVec, is_cosine, pts, wts, ctxt.b1, ctxt.Ylm);	
      genmat_detector_3D(ctxt.nodal_sphOrder, ctxt.node_angN, ctxt.offset, Detector, qmmesh, Ndetector, nM, pts, wts, ctxt.Ylm);
      //genmat_intsourcevalvector_3D(ctxt.nodal_sphOrder, ctxt.node_angN, ctxt.offset, b2, qmmesh, Nsource[0], dirVec, is_cosine, pts, wts, ctxt.Ylm);
     }
    else 
    {
      genmat_toastsource_3D(ctxt.nodal_sphOrder,  ctxt.node_angN, ctxt.offset, Source, qmmesh, qvec, ns, dirVec, is_cosine, pts, wts, ctxt.b1, ctxt.Ylm);
      genmat_toastdetector_3D(ctxt.nodal_sphOrder,  ctxt.node_angN, ctxt.offset, Detector, qmmesh, mvec, nM, pts, wts, ctxt.Ylm);
      //genmat_toastintsourcevalvector_3D(ctxt.nodal_sphOrder,  ctxt.node_angN, ctxt.offset, b2, qmmesh, qvec, 0, dirVec, is_cosine, pts, wts, ctxt.Ylm);
    }

    cout << "calculating the radiance\n";
    int sysdim = qmmesh.nlen();
    int fullsysdim = sum(ctxt.node_angN);
    AACP = new  CPrecon_Diag;
    CVector idiag = getDiag(&ctxt);
    AACP->ResetFromDiagonal(idiag);
   
    RHS = new CVector[ns];
    Phi = new CVector [ns];
    for (int j = 0; j < ns ; j++) {   
      Phi[j].New(fullsysdim);
      RHS[j].New(fullsysdim);
       for(int i = 0; i < fullsysdim; i++)
	 RHS[j][i] = Source[j].Get(i,0) + b2[i];
    }

    CVector *detect = new CVector[nM];
    for(int j=0; j<nM; j++)
    {
       detect[j].New(fullsysdim);
	for(int i =0; i < fullsysdim; i++)
		detect[j][i] = Detector[j].Get(i, 0);
    }	
    int num_thread_loops = (int)ceil((double)ns/NUM_THREADS);
    pthread_t thread[NUM_THREADS];
   
//GMRES(&matrixFreeCaller, &ctxt, RHS, Phi[0], tol, AACP, 100);
    int *thread_id = new int[NUM_THREADS];
    result = new CVector[NUM_THREADS];
    Xmat = new CCompRowMatrix[NUM_THREADS];
    Aintx = new CDenseMatrix[NUM_THREADS];
    Aintscx = new CDenseMatrix[NUM_THREADS];    
    Aintssx = new CDenseMatrix[NUM_THREADS];
    Aintcx = new CDenseMatrix[NUM_THREADS];    
    Aintscscx = new CDenseMatrix[NUM_THREADS];
    Aintscssx = new CDenseMatrix[NUM_THREADS];    
    Aintssssx = new CDenseMatrix[NUM_THREADS];
    Aintsccx = new CDenseMatrix[NUM_THREADS]; 
    Aintsscx = new CDenseMatrix[NUM_THREADS];
    Aintccx = new CDenseMatrix[NUM_THREADS];    
    apu1x = new CDenseMatrix[NUM_THREADS];
    apu1scx = new CDenseMatrix[NUM_THREADS]; 
    apu1ssx = new CDenseMatrix[NUM_THREADS];
    apu1cx = new CDenseMatrix[NUM_THREADS];  
    for(int i=0; i < NUM_THREADS; i++)
    {
	result[i].New(sum(ctxt.node_angN));
	Xmat[i].New(ctxt.spatN, ctxt.maxAngN);
	Xmat[i].Initialise(ctxt.xrowptr, ctxt.xcolidx);
	Aintx[i].New(ctxt.spatN, ctxt.maxAngN); Aintscx[i].New(ctxt.spatN, ctxt.maxAngN); Aintssx[i].New(ctxt.spatN, ctxt.maxAngN); 
	Aintcx[i].New(ctxt.spatN, ctxt.maxAngN); apu1x[i].New(ctxt.spatN, ctxt.maxAngN); apu1scx[i].New(ctxt.spatN, ctxt.maxAngN); 
	apu1ssx[i].New(ctxt.spatN, ctxt.maxAngN); apu1cx[i].New(ctxt.spatN, ctxt.maxAngN);Aintscscx[i].New(ctxt.spatN, ctxt.maxAngN); 
	Aintscssx[i].New(ctxt.spatN, ctxt.maxAngN); Aintssssx[i].New(ctxt.spatN, ctxt.maxAngN); Aintsccx[i].New(ctxt.spatN, ctxt.maxAngN);
	Aintsscx[i].New(ctxt.spatN, ctxt.maxAngN); Aintccx[i].New(ctxt.spatN, ctxt.maxAngN);
	thread_id[i] = i;
    }  
    //clock_t start = clock();
    time_t start, end;
    time(&start);
    for(int i=0; i < num_thread_loops; i++)
    {
	for(int j=0; j < NUM_THREADS; j++)
	{
		pthread_create(&thread[j], NULL, &solveForSource, (void *)&thread_id[j]);
			
	}
	for(int j=0; j<NUM_THREADS; j++)
	   pthread_join(thread[j], NULL);

    }
   time(&end);
   // clock_t end = clock();
    cout<<"The solver took "<<difftime(end, start)<<" seconds"<<endl;
    cout<<"Writing output files ..."<<endl;

    char fphi_re[300], fphi_im[300];
    strcpy(fphi_re, file_extn); strcpy(fphi_im, file_extn); 
    strcat(fphi_re, "_Phi_re.sol"); strcat(fphi_im, "_Phi_im.sol"); 
    
    char fRHS_re[300], fRHS_im[300];
    strcpy(fRHS_re, file_extn); strcpy(fRHS_im, file_extn);
    strcat(fRHS_re, "_RHS_re.sol"); strcat(fRHS_im, "_RHS_im.sol");
    
    FILE *osRHS_re, *osRHS_im, *osPhi_re, *osPhi_im;
    osRHS_re = fopen(fRHS_re, "w"); osRHS_im = fopen(fRHS_im, "w");
    osPhi_re = fopen(fphi_re, "w"); osPhi_im = fopen(fphi_im, "w");
    
    char fDet_re[300], fDet_im[300];
    strcpy(fDet_re, file_extn); strcpy(fDet_im, file_extn);
    strcat(fDet_re, "_Det_re.sol"); strcat(fDet_im, "_Det_im.sol");
    FILE *osDet_re, *osDet_im;
    osDet_re = fopen(fDet_re, "w"); osDet_im = fopen(fDet_im, "w");

    CVector *Phisum = new CVector [ns];
    for (int j = 0; j < ns ; j++) {   
      Phisum[j].New(sysdim);
      for(int i =0; i<fullsysdim; i++)
      {
	fprintf(osRHS_re, "%12e ", RHS[j][i].re);
	fprintf(osRHS_im, "%12e ", RHS[j][i].im);
	fprintf(osPhi_re, "%12e ", Phi[j][i].re);
	fprintf(osPhi_im, "%12e ", Phi[j][i].im);
	}
      fprintf(osRHS_re, "\n");fprintf(osRHS_im, "\n");
      fprintf(osPhi_re, "\n");fprintf(osPhi_im, "\n");
      for (int k = 0; k < sysdim; k++)
      {
	 Phisum[j][k] += Phi[j][ctxt.offset[k]]*sqrt(4*M_PI);
      }
      
    }
    fclose(osPhi_re);fclose(osPhi_im);
    fclose(osRHS_re);fclose(osRHS_im);

    for (int j = 0; j < nM ; j++) {   
	for(int i =0; i<fullsysdim; i++)
      	{
    		fprintf(osDet_re, "%12e ", detect[j][i].re);
		fprintf(osDet_im, "%12e ", detect[j][i].im);
	}
	fprintf(osDet_re, "\n");fprintf(osDet_im, "\n");
    }
    fclose(osDet_re);fclose(osDet_im);
    
    char flnmod[300], farg[300], ftime[300];
    strcpy(flnmod, file_extn); strcpy(farg, file_extn);strcpy(ftime, file_extn);
    strcat(flnmod, "_lnmod.nim"); strcat(farg, "_arg.nim"); strcat(ftime, "_time.txt");
    OpenNIM (flnmod, argv[1], sysdim);
    OpenNIM (farg, argv[1], sysdim);
    for (int i = 0; i < ns; i++) {
	   WriteNIM (flnmod, LogMod(Phisum[i]), sysdim, i);
	   WriteNIM (farg, Arg(Phisum[i]), sysdim, i);
    }
    cout << "  Log Mod field written to "<< flnmod << endl;
    cout << "  Arg field written to "<<farg << endl;

    FILE *fid;
    fid = fopen(ftime, "w");
    fprintf(fid, "Time taken by solver: %f\n", difftime(end, start));
    fclose(fid);
    
    delete []Phi;
    delete []RHS;
    delete []Phisum;
    delete []Nsource;
    delete []Ndetector;
    delete []Source;
    delete []Detector;
    delete []result;
    delete []Xmat;
    delete []Aintx;
    delete []Aintscx;
    delete []Aintssx;
    delete []Aintcx;
    delete []Aintscscx;
    delete []Aintscssx;
    delete []Aintssssx;
    delete []Aintsccx;
    delete []Aintsscx;
    delete []Aintccx;
    delete []apu1x;
    delete []apu1scx;
    delete []apu1ssx;
    delete []apu1cx;

}

void *solveForSource(void *thread_id)
{
	//int *sid = (int *)source_idx;
        int source_num;
	int thread_num;
	pthread_mutex_lock(&sid_mutex);
	int *tid = (int *)thread_id;
	if(sources_processed < ns-1)
	{
		sources_processed++;
		source_num = sources_processed;
	        thread_num = *tid;	
		pthread_mutex_unlock(&sid_mutex);
		cout<<"Processing source number: "<<source_num<< "  "<<thread_num<<"  "<<*tid<<endl;

	}
	else 
	{
	    pthread_mutex_unlock(&sid_mutex);
	    return 0;
	}
	 GMRES(&matrixFreeCaller, &thread_num, RHS[source_num], Phi[source_num], tol, AACP, (int)ceil(100.0/NUM_THREADS));
	
}

inline CVector matrixFreeCaller(const CVector& x, void *tid)
{
	int *thread_num = (int *)tid;  
	int dof = sum(ctxt.node_angN);
	int spatN = ctxt.spatN;
	int maxAngN = ctxt.maxAngN;
	
	int tidx = *thread_num;
	/*Implict Kronecker product implementation
	*	(S \circplus A)x = Sx_{r}A^{T}
	* where 'x_{r}' is a matrix resulting form reshaping of 'x'. 
	*/

	/*Reshaping 'x' to 'x_{r}'*/
	toast::complex *xmatval = Xmat[tidx].ValPtr();
	
	memcpy (xmatval, x.data_buffer(), dof*sizeof(toast::complex));
	int i, j, k, m, ra, ra1, ra2, rb, rb1, rb2;
    	int nr = Xmat[tidx].nRows();
    	int nc = ctxt.Aint.nCols();
   
	toast::complex *aintxval = Aintx[tidx].data_buffer(); toast::complex *aintscxval = Aintscx[tidx].data_buffer(); toast::complex *aintssxval = Aintssx[tidx].data_buffer();
	toast::complex *aintcxval = Aintcx[tidx].data_buffer(); toast::complex *apu1xval = apu1x[tidx].data_buffer(); toast::complex *apu1scxval = apu1scx[tidx].data_buffer();  
	toast::complex *apu1ssxval = apu1ssx[tidx].data_buffer(); toast::complex *apu1cxval = apu1cx[tidx].data_buffer(); 
	toast::complex *aintscscxval = Aintscscx[tidx].data_buffer(); toast::complex *aintscssxval = Aintscssx[tidx].data_buffer();  
	toast::complex *aintssssxval = Aintssssx[tidx].data_buffer(); toast::complex *aintsccxval = Aintsccx[tidx].data_buffer();  
	toast::complex *aintsscxval = Aintsscx[tidx].data_buffer();  toast::complex *aintccxval = Aintccx[tidx].data_buffer();  

	 
	/*Intialize Ax's to zero where A is the angular matrix*/	
	memset(aintxval, 0, dof*sizeof(toast::complex)); memset(aintscxval, 0, dof*sizeof(toast::complex)); 
	memset(aintssxval, 0, dof*sizeof(toast::complex)); memset(aintcxval, 0, dof*sizeof(toast::complex)); 
	memset(aintscscxval, 0, dof*sizeof(toast::complex)); memset(aintscssxval, 0, dof*sizeof(toast::complex)); 
	memset(aintsccxval, 0, dof*sizeof(toast::complex)); memset(aintsscxval, 0, dof*sizeof(toast::complex));
	memset(aintssssxval, 0, dof*sizeof(toast::complex)); memset(aintccxval, 0, dof*sizeof(toast::complex)); 
	memset(apu1xval, 0, dof*sizeof(toast::complex)); memset(apu1scxval, 0, dof*sizeof(toast::complex)); 
	memset(apu1ssxval, 0, dof*sizeof(toast::complex)); memset(apu1cxval, 0, dof*sizeof(toast::complex));
	/*Dereference the val pointers of Ax's*/	
	
	/*Computing x_{r}A^{T}*/
	toast::complex xval;
    	for (i = 0; i < nr; i++) {
    		ra1 = Xmat[tidx].rowptr[i];
		ra2 = Xmat[tidx].rowptr[i+1];
		for (ra = ra1; ra < ra2; ra++) {
			j = Xmat[tidx].colidx[ra];
			xval = xmatval[ra];

	    		rb1 = ctxt.Aint.rowptr[j];
	    		rb2 = ctxt.Aint.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aint.colidx[rb];
				aintxval[i*maxAngN + k] += xval*ctxt.aintval[rb];		
	    		}

			rb1 = ctxt.Aintsc.rowptr[j];
	    		rb2 = ctxt.Aintsc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintsc.colidx[rb];
				aintscxval[i*maxAngN + k] += xval*ctxt.aintscval[rb];		
	    		}

			rb1 = ctxt.Aintss.rowptr[j];
	    		rb2 = ctxt.Aintss.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintss.colidx[rb];
				aintssxval[i*maxAngN + k] += xval*ctxt.aintssval[rb];		
	    		}
			
			rb1 = ctxt.Aintc.rowptr[j];
	    		rb2 = ctxt.Aintc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintc.colidx[rb];
				aintcxval[i*maxAngN + k] += xval*ctxt.aintcval[rb];		
	    		}

			rb1 = ctxt.apu1.rowptr[j];
	    		rb2 = ctxt.apu1.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.apu1.colidx[rb];
				apu1xval[i*maxAngN + k] += xval*ctxt.apu1val[rb];		
	    		}

			rb1 = ctxt.apu1sc.rowptr[j];
	    		rb2 = ctxt.apu1sc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.apu1sc.colidx[rb];
				apu1scxval[i*maxAngN + k] += xval*ctxt.apu1scval[rb];		
	    		}

			rb1 = ctxt.apu1ss.rowptr[j];
	    		rb2 = ctxt.apu1ss.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.apu1ss.colidx[rb];
				apu1ssxval[i*maxAngN + k] += xval*ctxt.apu1ssval[rb];		
	    		}
			
			rb1 = ctxt.apu1c.rowptr[j];
	    		rb2 = ctxt.apu1c.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.apu1c.colidx[rb];
				apu1cxval[i*maxAngN + k] += xval*ctxt.apu1cval[rb];		
	    		}

			rb1 = ctxt.Aintscsc.rowptr[j];
	    		rb2 =ctxt.Aintscsc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintscsc.colidx[rb];
				aintscscxval[i*maxAngN + k] += xval*ctxt.aintscscval[rb];		
	    		}

			rb1 = ctxt.Aintscss.rowptr[j];
	    		rb2 = ctxt.Aintscss.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintscss.colidx[rb];
				aintscssxval[i*maxAngN + k] += xval*ctxt.aintscssval[rb];		
	    		}

			rb1 = ctxt.Aintssss.rowptr[j];
	    		rb2 = ctxt.Aintssss.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintssss.colidx[rb];
				aintssssxval[i*maxAngN + k] += xval*ctxt.aintssssval[rb];		
	    		}
			
			rb1 = ctxt.Aintscc.rowptr[j];
	    		rb2 = ctxt.Aintscc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintscc.colidx[rb];
				aintsccxval[i*maxAngN + k] += xval*ctxt.aintsccval[rb];		
	    		}
			
			rb1 = ctxt.Aintssc.rowptr[j];
	    		rb2 = ctxt.Aintssc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintssc.colidx[rb];
				aintsscxval[i*maxAngN + k] += xval*ctxt.aintsscval[rb];		
	    		}
			
			rb1 = ctxt.Aintcc.rowptr[j];
	    		rb2 = ctxt.Aintcc.rowptr[j+1];
	    		for (rb = rb1; rb < rb2; rb++) {
	        		k = ctxt.Aintcc.colidx[rb];
				aintccxval[i*maxAngN + k] += xval*ctxt.aintccval[rb];		
	    		}


		}
    }
    /*Computing S(x_{r}A^{T})*/
    int scol;
    for(int is = 0; is < spatN; is++)
    {
	for(int ia = 0; ia < ctxt.node_angN[is]; ia++)
	{
		toast::complex temp(0, 0);
		for(int js = ctxt.SAint_co.rowptr[is]; js < ctxt.SAint_co.rowptr[is+1]; js++)
		{
			scol = ctxt.SAint_co.colidx[js];
			temp += ctxt.saint_coval[js]*aintxval[scol*maxAngN + ia];
			temp += ctxt.saintsc_coval[js]*aintscxval[scol*maxAngN + ia];
			temp += ctxt.saintss_coval[js]*aintssxval[scol*maxAngN + ia];
			temp += ctxt.saintc_coval[js]*aintcxval[scol*maxAngN +  ia];
			temp += ctxt.sdxxval[js]*aintscscxval[scol*maxAngN + ia];
			temp += ctxt.saintscss_coval[js]*aintscssxval[scol*maxAngN + ia];
			temp += ctxt.sdyyval[js]*aintssssxval[scol*maxAngN + ia];
			temp += ctxt.saintscc_coval[js]*aintsccxval[scol*maxAngN + ia];
			temp += ctxt.saintssc_coval[js]*aintsscxval[scol*maxAngN + ia];
			temp += ctxt.sdzzval[js]*aintccxval[scol*maxAngN + ia];
			temp -= ctxt.spsval[js]*apu1xval[scol*maxAngN + ia];
			temp -= ctxt.spsdxval[js]*apu1scxval[scol*maxAngN + ia];
			temp -= ctxt.spsdyval[js]*apu1ssxval[scol*maxAngN +  ia];
			temp -= ctxt.spsdzval[js]*apu1cxval[scol*maxAngN +  ia];
		}
		result[tidx][ctxt.offset[is] + ia]  = temp;
	} 
	}
	/*Computing A_{2}x explicitly where A_{2} is the matrix resulting from boundary*/
  	int r, i2;
    	toast::complex br;

    	for (r = i = 0; r < ctxt.A2.nRows();) {
		i2 = ctxt.A2.rowptr[r+1];
		for (br = toast::complex(0, 0); i < i2; i++)
	    		br += x[ctxt.A2.colidx[i]]*ctxt.a2val[i];
		result[tidx][r++] += br;
    	}

    	return result[tidx];
}
/** Computes the diagonal of the system matrix which is required for preconditioning
**/
CVector getDiag(void * context)
{
    MyDataContext *ctxt = (MyDataContext*) context;
    int spatN = ctxt->spatN; 
    int nDim = sum(ctxt->node_angN);
    CVector result(nDim);
    int arow, brow;
    toast::complex a;
    double  coeff = ctxt->w/ctxt->c;

    for(int i = 0; i < spatN; i++)
    {
	for(int j = 0; j < ctxt->node_angN[i]; j++)
	{
		a = ctxt->SAint_co.Get(i, i)*ctxt->Aint.Get(j, j);
		a += ctxt->SAintsc_co.Get(i, i)*ctxt->Aintsc.Get(j, j);
		a += ctxt->SAintss_co.Get(i, i)*ctxt->Aintss.Get(j, j);
		a += ctxt->SAintc_co.Get(i, i)*ctxt->Aintc.Get(j, j);
		a +=ctxt->SAintscss_co.Get(i, i)*ctxt->Aintscss.Get(j, j);
		a += ctxt->SAintscc_co.Get(i, i)*ctxt->Aintscc.Get(j, j);
		a += ctxt->SAintssc_co.Get(i, i)*ctxt->Aintssc.Get(j, j);
		a += ctxt->Sdxx.Get(i, i)*ctxt->Aintscsc.Get(j, j);
		a +=ctxt->Sdyy.Get(i, i)*ctxt->Aintssss.Get(j, j);	
		a += ctxt->Sdzz.Get(i, i)*ctxt->Aintcc.Get(j, j);
		a -= ctxt->SPS.Get(i, i)*ctxt->apu1.Get(j, j);
		a -= ctxt->SPSdx.Get(i, i)*ctxt->apu1sc.Get(j, j);
		a -= ctxt->SPSdy.Get(i, i)*ctxt->apu1ss.Get(j, j);
		a -= ctxt->SPSdz.Get(i, i)*ctxt->apu1c.Get(j, j);
		a += ctxt->A2.Get(ctxt->offset[i]+j, ctxt->offset[i]+j);

	
		result[ctxt->offset[i] + j] = a;
	}
     }
     return result;
}


//=========================================================================


void SelectSourceProfile (int &qtype, double &qwidth, SourceMode &srctp)
{
    char cbuf[256];
    int cmd;

    bool typeok = false;
    if (pp.GetString ("SOURCETYPE", cbuf)) {
	if (!strcasecmp (cbuf, "NEUMANN")) {
	    srctp = SRCMODE_NEUMANN;
	    typeok = true;
	} else if (!strcasecmp (cbuf, "ISOTROPIC")) {
	    srctp = SRCMODE_ISOTROPIC;
	    typeok = true;
	}
    }
    while (!typeok) {
	cout << "\nSource type:\n";
	cout << "(1) Neumann boundary source\n";
	cout << "(2) Isotropic point source\n";
	cout << "[1|2] >> ";
	cin  >> cmd;
	cout<<cmd<<endl;
	switch (cmd) {
	    case 1: srctp = SRCMODE_NEUMANN;   typeok = true; break;
	    case 2: srctp = SRCMODE_ISOTROPIC; typeok = true; break;
	}
    }
    pp.PutString ("SOURCETYPE",
        srctp == SRCMODE_NEUMANN ? "NEUMANN" : "ISOTROPIC");

    qtype = -1;
    if (pp.GetString ("SOURCEPROFILE", cbuf)) {
	if (!strcasecmp (cbuf, "POINT")) {
	    qtype = 0;
	} else if (!strcasecmp (cbuf, "GAUSSIAN")) {
	    qtype = 1;
	} else if (!strcasecmp (cbuf, "COSINE")) {
	    qtype = 2;
	}
    }
    while (qtype < 0) {
	cout << "\nSource profile:\n";
	cout << "(1) Point\n";
	cout << "(2) Gaussian\n";
	cout << "(3) Cosine\n";
	cout << "[1|2|3] >> ";
	cin  >> qtype;
	cout<<qtype<<endl;
	qtype -= 1;
    }
    if (qtype > 0 && !pp.GetReal ("SOURCEWIDTH", qwidth)) {
	switch (qtype) {
	case 1:
	    cout << "\nSource 1/e radius [mm]:\n>> ";
	    break;
	case 2:
	    cout << "\nSource support radius [mm]:\n>> ";
	    break;
	}
	cin >> qwidth;
	cout<<qwidth<<endl;
    }
    switch (qtype) {
    case 0:
	pp.PutString ("SOURCEPROFILE", "POINT");
	break;
    case 1:
	pp.PutString ("SOURCEPROFILE", "GAUSSIAN");
	pp.PutReal ("SOURCEWIDTH", qwidth);
	break;
    case 2:
	pp.PutString ("SOURCEPROFILE", "COSINE");
	pp.PutReal ("SOURCEWIDTH", qwidth);
	break;
    }
}
// ============================================================================

void SelectMeasurementProfile (ParamParser &pp, int &mtype, double &mwidth)
{
    char cbuf[256];
    mtype = -1;
    if (pp.GetString ("MEASUREMENTPROFILE", cbuf)) {
	if (!strcasecmp (cbuf, "POINT")) {
	    mtype = 0;
	} else if (!strcasecmp (cbuf, "GAUSSIAN")) {
	    mtype = 1;
	} else if (!strcasecmp (cbuf, "COSINE")) {
	    mtype = 2;
	}
    }
    while (mtype < 0) {
	cout << "\nMeasurement profile:\n";
	cout << "(1) Point\n";
	cout << "(2) Gaussian\n";
	cout << "(3) Cosine\n";
	cout << "[1|2|3] >> ";
	cin  >> mtype;
	cout << mtype<<endl;
	mtype -= 1;
    }
    if (mtype > 0 && !pp.GetReal ("MEASUREMENTWIDTH", mwidth)) {
	switch (mtype) {
	case 1:
	    cout << "\nMeasurement 1/e radius [mm]:\n>> ";
	    break;
	case 2:
	    cout << "\nMeasurement support radius [mm]:\n>> ";
	    break;
	}
	cin >> mwidth;
	cout << mwidth << endl;
    }
    switch (mtype) {
    case 0:
	pp.PutString ("MEASUREMENTPROFILE", "POINT");
	break;
    case 1:
	pp.PutString ("MEASUREMENTPROFILE", "GAUSSIAN");
	pp.PutReal ("MEASUREMENTWIDTH", mwidth);
	break;
    case 2:
	pp.PutString ("MEASUREMENTPROFILE", "COSINE");
	pp.PutReal ("MEASUREMENTWIDTH", mwidth);
	break;
    }
}
void WriteData (const RVector &data, char *fname)
{
    ofstream ofs (fname);
    ofs << setprecision(14);
    ofs << data << endl;
}

void WriteDataBlock (const QMMesh &mesh, const RVector &data, char *fname)
{
    int q, m, i;
    ofstream ofs (fname);
    for (m = i = 0; m < mesh.nM; m++) {
	for (q = 0; q < mesh.nQ; q++) {
	    if (mesh.Connected (q,m)) ofs << data[i++];
	    else                      ofs << '-';
	    ofs << (q == mesh.nQ-1 ? '\n' : '\t');
	}
    }   
}

void OpenNIM (const char *nimname, const char *meshname, int size)
{
    ofstream ofs (nimname);
    ofs << "NIM" << endl;
    ofs << "Mesh = " << meshname << endl;
    ofs << "SolutionType = N/A" << endl;
    ofs << "ImageSize = " << size << endl;
    ofs << "EndHeader" << endl;
}

void WriteNIM (const char *nimname, const RVector &img, int size, int no)
{
    ofstream ofs (nimname, ios::app);
    ofs << "Image " << no << endl;
    for (int i = 0; i < size; i++)
        ofs << img[i] << ' ';
    ofs << endl;
}

bool ReadNim (char *nimname, RVector &img)
{
    char cbuf[256];
    int i, imgsize = 0;

    ifstream ifs (nimname);
    if (!ifs.getline (cbuf, 256)) return false;
    if (strcmp (cbuf, "NIM")) return false;
    do {
        ifs.getline (cbuf, 256);
	if (!strncasecmp (cbuf, "ImageSize", 9))
	    sscanf (cbuf+11, "%d", &imgsize);
    } while (strcasecmp (cbuf, "EndHeader"));
    if (!imgsize) return false;
    img.New(imgsize);
    do {
        ifs.getline (cbuf, 256);
    } while (strncasecmp (cbuf, "Image", 5));
    for (i = 0; i < imgsize; i++)
        ifs >> img[i];
    return true;
}

#ifdef WRITEPPM
void WritePGM (const RVector &img, const IVector &gdim, char *fname)
{
    int i, ii, dim = gdim[0]*gdim[1];
    double imgmin = 1e100, imgmax = -1e100;
    unsigned char *pixmap = new unsigned char[dim];

    if (gdim.Dim() > 2)
        ii = (gdim[2]/2)*gdim[0]*gdim[1]; // simply plot middle layer
    else
        ii = 0;

    for (i = 0; i < dim; i++) {
        if (img[i+ii] < imgmin) imgmin = img[i+ii];
	if (img[i+ii] > imgmax) imgmax = img[i+ii];
    }
    double scale = 256.0/(imgmax-imgmin);
    for (i = 0; i < dim; i++) {
        int v = (int)((img[i+ii]-imgmin)*scale);
	if      (v < 0  ) v = 0;
	else if (v > 255) v = 255;
	pixmap[i] = (unsigned char)v;
    }
    ofstream ofs(fname);
    ofs << "P5" << endl;
    ofs << "# CREATOR: TOAST (test_grid)" << endl;
    ofs << gdim[0] << ' ' << gdim[1] << endl;
    ofs << 255 << endl;
    for (i = 0; i < dim; i++) ofs << pixmap[i];
    ofs << endl;
    delete []pixmap;
}

void WritePPM (const RVector &img, const IVector &gdim,
    double *scalemin, double *scalemax, char *fname)
{
    typedef struct {
        unsigned char r,g,b;
    } RGB;

    int i, ii, dim = gdim[0]*gdim[1];
    unsigned char *pixmap = new unsigned char[dim];
    double imgmin, imgmax, scale;

    static RGB colmap[256];
    static bool have_colmap = false;

    if (!have_colmap) {
        int r, g, b;
        ifstream ifs (colormap);
	for (i = 0; i < 256; i++) {
	    ifs >> r >> g >> b;
	    colmap[i].r = (unsigned char)r;
	    colmap[i].g = (unsigned char)g;
	    colmap[i].b = (unsigned char)b;
	}
	have_colmap = true;
    }

    if (gdim.Dim() > 2)
        ii = (gdim[2]/2)*gdim[0]*gdim[1]; // simply plot middle layer
    else
        ii = 0;

    // rescale image
    if (scalemin) {
        imgmin = *scalemin;
    } else {
	for (i = 0, imgmin = 1e100; i < dim; i++)
	    if (img[i+ii] < imgmin) imgmin = img[i+ii];
    }
    if (scalemax) {
        imgmax = *scalemax;
    } else {
      for (i = 0, imgmax = -1e100; i < dim; i++)
	  if (img[i+ii] > imgmax) imgmax = img[i+ii];
    }
    scale = 256.0/(imgmax-imgmin);

    for (i = 0; i < dim; i++) {
        int v = (int)((img[i+ii]-imgmin)*scale);
	if      (v < 0  ) v = 0;
	else if (v > 255) v = 255;
	pixmap[i] = (unsigned char)v;
    }
    ofstream ofs(fname);
    ofs << "P6" << endl;
    ofs << "# CREATOR: TOAST (test_grid)" << endl;
    ofs << gdim[0] << ' ' << gdim[1] << endl;
    ofs << 255 << endl;
    for (i = 0; i < dim; i++)
        ofs << colmap[pixmap[i]].r
	    << colmap[pixmap[i]].g
	    << colmap[pixmap[i]].b;
    ofs << endl;

    delete []pixmap;
}

#endif
