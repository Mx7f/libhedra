// This file is part of libhedra, a library for polyhedral mesh processing
//
// Copyright (C) 2016 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef HEDRA_AFFINE_MAPS_DEFORM_H
#define HEDRA_AFFINE_MAPS_DEFORM_H

#include <igl/igl_inline.h>
#include <Eigen/Core>
#include <vector>
#include <igl/min_quad_with_fixed.h>

//This file implements the deformation algorithm described in:

//Amir Vaxman
//Modeling Polyhedral Meshes with Affine Maps
//Computer Graphics Forum (Proc. SGP) 31(5), 2012

namespace hedra
{
    // Deform a polyhedral mesh with a single affine map per face
    
    enum AffineEnergyTypes{
        ARAP,  //as-rigid-as-possible energy
        ASAP  //as-similar-as-possible energy ("conformal")
    };
    
    struct AffineData{
        Eigen::SparseMatrix<double> E;  //energy matrix
        Eigen::SparseMatrix<double> C;  //constraint matrix
        igl::min_quad_with_fixed_data<double> mqwfd;  //data with the quadratic solver
        AffineEnergyTypes aet;
        double bendFactor;
        int FSize, VSize;
    };
    
    
    //precomputation of the necessary matrices
    
    //input:
    //  V  eigen double matrix  #V by 3 original mesh coordinates
    //  D  eigen int vector     #F by 1 - face degrees
    //  F  eigen int matrix     #F by max(D) - vertex indices in face
    //  EF eigen int matrix     #E by 2 - map from edges to adjacent faces
    //  EV eigen int matrix     #E by 2 - map from edges to end vertices
    //  h eigen int vector      #constraint vertex indices (handles)
    //  bendFactor double       #the relative similarty between affine maps on adjacent faces
  
    // Output:
    // adata struct AffineData     the data necessary to solve the linear system.

    //TODO: Currently uniform weights. Make them geometric.
    IGL_INLINE void affine_maps_precompute(const Eigen::MatrixXd& V,
                                           const Eigen::VectorXi& D,
                                           const Eigen::MatrixXi& F,
                                           const Eigen::MatrixXi& EF,
                                           const Eigen::MatrixXi& EV,
                                           const Eigen::VectorXi& h,
                                           struct AffineData& adata)
    {
        
        
        //Assembling the constraint matrix C
        int CRows=0;
        int NumVars=3*F.rows()+V.rows();  //every dimension is seperable.
        
        std::vector<Eigen::Triplet<double> > CTripletList;
        for(int i=0;i<EF.rows();i++)
        {
            Eigen::RowVector3d EdgeVector=V.row(EV(i,1))-V.row(EV(i,0));
            for (int j=0;j<2;j++){
                if (EF(i,j)!=-1)
                    for (int k=0;k<3;k++){
                        CTripletList.push_back(Eigen::Triplet<double>(CRows,3*EF(i,j)+k,EdgeVector(k)));
                        CTripletList.push_back(Eigen::Triplet<double>(CRows,3*D.size()+EV(i,0),-1.0));
                        CTripletList.push_back(Eigen::Triplet<double>(CRows,3*D.size()+EV(i,1),1.0));
                    }
                CRows++;
            }
            
        }
        
    
        //Assembling the energy matrix E
        int ERows=0;
        //NumVars is the same
        std::vector<Eigen::Triplet<double> > ETripletList;
        
        //prescription to a given matrix per face - just an identity
        //TODO: stack up a stock identity matrix instead of wording it here?
        for(int i=0;i<3*F.rows();i++){
            ETripletList.push_back(Eigen::Triplet<double>(i,i,1.0));
        }
        ERows+=3*F.rows();
        
        //"bending" energy to difference of adjacent matrices
        for(int i=0;i<EF.rows();i++)
        {
            if ((EF(i,0)==-1)||(EF(i,1)==-1))  //boundary edge
                continue;
            
            for (int k=0;k<3;k++){
                ETripletList.push_back(Eigen::Triplet<double>(ERows,3*EF(i,0)+k,-1.0));
                ETripletList.push_back(Eigen::Triplet<double>(ERows,3*EF(i,1)+k,1.0));
            }
            ERows++;
        }
        
       
        adata.E.resize(ERows,NumVars);
        adata.E.setFromTriplets(ETripletList.begin(), ETripletList.end());
        adata.C.resize(CRows,NumVars);
        adata.C.setFromTriplets(CTripletList.begin(), CTripletList.end());
        adata.FSize=F.rows();
        adata.VSize=V.rows();
        igl::min_quad_with_fixed_precompute<double>(adata.E, h,adata.C,true,adata.mqwfd);
    }
    
    
    //Computing the deformation.
    //Prerequisite: affine_maps_precompute is called, and
    //qh input values are matching those in h.
    
    //input:
    // adata struct AffineData     the data necessary to solve the linear system.
    // qh eigen double matrix           h by 3 new handle positions
    // q0 eigen double matrix           v by 3 initial solution
    
    //output:
    // A eigen double matrix            3*F by 3 affine maps (stacked 3x3 per face)
    // q eigen double matrix            V by 3 new vertex positions (note: include handles)
    
    
    //currently: solving only one global system (thus, initial solution is not used).
    IGL_INLINE void affine_maps_deform(struct AffineData& adata,
                                       const Eigen::MatrixXd& qh,
                                       const Eigen::MatrixXd& q0,
                                       Eigen::MatrixXd A,
                                       Eigen::MatrixXd q)
    {
    
        Eigen::MatrixXd RawResult;
        //currently not working!!!
        igl::min_quad_with_fixed_solve(adata.mqwfd,Eigen::VectorXd::Zero(adata.E.cols()),qh,Eigen::VectorXd::Zero(adata.C.rows()),RawResult);
        
        A=RawResult.block(0,0,3*adata.FSize,3);
        q=RawResult.block(3*adata.FSize,0,adata.VSize,3);
    }
}


#endif
