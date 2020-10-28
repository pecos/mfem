#ifndef HYPSYS_HYPERBOLIC_SYSTEM
#define HYPSYS_HYPERBOLIC_SYSTEM

#include "../lib/tools.hpp"

struct Configuration
{
   int ConfigNum;
   double tFinal;
   Vector bbMin, bbMax;
};

class HyperbolicSystem
{
public:
   explicit HyperbolicSystem(FiniteElementSpace *fes_, BlockVector &u_block,
                             int NumEq_, Configuration &config_,
                             VectorFunctionCoefficient BdrCond_) : fes(fes_), u0(fes_, u_block),
      NumEq(NumEq_), BdrCond(BdrCond_)
   {
      ne = fes->GetNE();
      nd = fes->GetFE(0)->GetDof();
      dim = fes->GetMesh()->Dimension();

      l2_fec = new L2_FECollection(fes->GetFE(0)->GetOrder(),
                                   fes->GetMesh()->Dimension());
      l2_fes = new FiniteElementSpace(fes->GetMesh(), l2_fec, NumEq,
                                      Ordering::byNODES);
      l2_proj = new GridFunction(l2_fes);
   }

   virtual ~HyperbolicSystem()
   {
      delete l2_proj;
      delete l2_fes;
      delete l2_fec;
   }

   virtual void EvaluateFlux(const Vector &u, DenseMatrix &FluxEval,
                             int e, int k, int i = -1) const = 0;
   virtual double GetGMS(const Vector &uL, const Vector &uR,
                         const Vector &normal) const { } // TODO: if used "= 0"
   virtual double GetWaveSpeed(const Vector &u, const Vector n, int e, int k,
                               int i = -1) const = 0;
   virtual void CheckAdmissibility(const Vector &u) const { };
   virtual void SetBdrCond(const Vector &y1, Vector &y2, const Vector &normal,
                           int attr) const  { };
   virtual void ComputeDerivedQuantities(const GridFunction &u, GridFunction &d1,
                                         GridFunction &d2) const { };
   virtual void ComputeErrors(Array<double> &errors, const GridFunction &u,
                              double DomainSize, double t) const { };

   virtual void WriteErrors(const Array<double> &errors) const
   {
      ofstream file("errors.txt", ios_base::app);

      if (!file)
      {
         MFEM_ABORT("Error opening file.");
      }
      else
      {
         ostringstream strs;
         for (int i = 0; i < errors.Size(); i++)
         {
            strs << errors[i] << " ";
         }
         strs << "\n";
         string str = strs.str();
         file << str;
         file.close();
      }
   }

   // L2 projection for scalar problems.
   void L2_Projection(FunctionCoefficient fun, GridFunction &proj) const
   {
      l2_proj->ProjectCoefficient(fun);
      proj.ProjectGridFunction(*l2_proj);
   }

   // L2 projection for systems.
   void L2_Projection(VectorFunctionCoefficient fun, GridFunction &proj) const
   {
      l2_proj->ProjectCoefficient(fun);
      proj.ProjectGridFunction(*l2_proj);
   }

   // Lumped L2 projection for general problems.
   void LumpedL2_Projection(VectorFunctionCoefficient fun, GridFunction &proj) const
   {
      Vector LumpedMassMat;

      Vector aux_vec(NumEq);
      aux_vec = 1.0;
      VectorConstantCoefficient ones(aux_vec);
      BilinearForm ml(fes);
      ml.AddDomainIntegrator(new LumpedIntegrator(new VectorMassIntegrator(ones)));
      ml.Assemble();
      ml.Finalize();
      ml.SpMat().GetDiag(LumpedMassMat);

      LinearForm rhs(fes);
      rhs.AddDomainIntegrator(new VectorDomainLFIntegrator(fun));
      rhs.Assemble();

      for (int i = 0; i < LumpedMassMat.Size(); i++)
      {
         proj(i) = rhs.Elem(i) / LumpedMassMat(i);
      }
   }

   int ne, nd, dim;

   // 0: L2 projection,
   // 1: Nodal values as GridFunction coefficients (only second order accurate).
   int ProjType;
   const int NumEq;

   FiniteElementSpace *fes;
   GridFunction u0;

   // Auxiliary data needed for L2 projections
   L2_FECollection *l2_fec;
   FiniteElementSpace *l2_fes;
   GridFunction *l2_proj;

   mutable VectorFunctionCoefficient BdrCond;

   string ProblemName, glvis_scale;
   bool SolutionKnown;
   bool SteadyState;
   bool TimeDepBC;

   // Currently only true for advection, due to spatially dependent flux.
   bool DiscreteUpwinding = false;
   DenseTensor VelNode;
};

#endif
