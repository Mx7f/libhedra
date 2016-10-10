// This file is part of libhedra, a library for polyhedral mesh processing
//
// Copyright (C) 2016 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef HEDRA_OFFSET_MESH_TRAITS_H
#define HEDRA_OFFSET_MESH_TRAITS_H
#include <igl/igl_inline.h>
#include <igl/harmonic.h>
#include <Eigen/Core>
#include <string>
#include <vector>
#include <cstdio>
#include <set>


namespace hedra { namespace optimization {
        
        //this class is a traits class for computing an approximate distance "d" offset (but exact parallel) for a given mesh. Alternatively, this is a discrete Gauss map. The possibilities are face, vertex, and edge offset, where the specific offset types refer to the distance of the respective components.
    
        class OffsetMeshTraits{
        public:
            
            typedef enum {VERTEX_OFFSET, EDGE_OFFSET, FACE_OFFSET} OffsetType;
            //Requisites of the traits class
            //for the energy
            Eigen::VectorXi JERows, JECols;  //rows and column indices for the jacobian matrix
            Eigen::VectorXd JEVals;         //values for the jacobian matrix.
            Eigen::VectorXd EVec;          //energy vector
            int xSize;                  //size of the solution
            //for the constraints
            Eigen::VectorXi JCRows, JCCols;  //rows and column indices for the jacobian matrix
            Eigen::VectorXd JCVals;         //values for the jacobian matrix.
            Eigen::VectorXd CVec;          //energy vector
            
            Eigen::MatrixXd VOrig;
            Eigen::MatrixXi F, EV;
            Eigen::VectorXi D;  //libhedra mesh representation
            Eigen::SparseMatrix<double> offsetConstMat;  //contains the linear offset constraint matrix
            OffsetType oType;
            double d;                           //the requested offset

            Eigen::MatrixXd fullSolution;       //The final solution of the last optimization
            
            void init(const Eigen::MatrixXd& _VOrig,
                      const Eigen::VectorXi& _D,
                      const Eigen::MatrixXi& _F,
                      const Eigen::MatrixXi& _EV,
                      OffsetType _oType,
                      const double _d){
                
                using namespace std;
                using namespace Eigen;
                
                std::set<pair<int, int> > edgeIndicesList;
                
                VOrig=_VOrig;
                F=_F;
                D=_D;
                EV=_EV;
                oType=_oType;
                d=_d;
                
                xSize=3*VOrig.rows()+EV.rows();
                
                //constructing offset constraint matrix and Jacobian values
                CVec.resize(3*EV.rows());
                vector<Triplet<double> > offsetTriList;
                //the constraints are of the time v'2-v'1 = s*(v2-v1)
                for (int i=0;i<EV.rows();i++){
                    RowVector3d origEdge=VOrig.row(EV(i,1))-VOrig.row(EV(i,0));
                    for (int j=0;j<3;j++){
                        //-v'1
                        offsetTriList.push_back(Triplet<double>(3*i+j, 3*EV(i,0)+j, -1.0));
                        //v'2
                        offsetTriList.push_back(Triplet<double>(3*i+j, 3*EV(i,1)+j, 1.0));
                        //-s
                        offsetTriList.push_back(Triplet<double>(3*i+j, 3*VOrig.rows()+i, -origEdge(j)));
                    }
                }
                
                offsetConstMat.resize(3*EV.rows(), xSize);
                offsetConstMat.setFromTriplets(offsetTriList.begin(), offsetTriList.end());
                JCRows.resize(offsetTriList.size());
                JCCols.resize(offsetTriList.size());
                JCVals.resize(offsetTriList.size());
                
                int currIndex=0;
                for (vector<Triplet<double> >::iterator oi=offsetTriList.begin(); oi!=offsetTriList.end();oi++){
                    JCRows(currIndex)=oi->row();
                    JCCols(currIndex)=oi->col();
                    JCVals(currIndex++)=oi->value();
                }
                
                //constructing energy jacobian values. It depends on the specific offset
                if (oType==VERTEX_OFFSET){
                    
                    //the energy is of the type \sum_v{(V-VOrig)^2-d^2},
                    EVec.resize(VOrig.rows());
                    JERows.resize(3*VOrig.rows());
                    JECols.resize(3*VOrig.rows());
                    JEVals.resize(3*VOrig.rows());
                    for (int i=0;i<VOrig.rows();i++){
                        for (int j=0;j<3;j++){
                            JERows(3*i+j)=i;
                            JECols(3*i+j)=3*i+j;
                        }
                    }
                }
                
                if (oType==EDGE_OFFSET){
                    //currently not supporting
                }
                if (oType==FACE_OFFSET){
                    //currently not supporting
                }
            }
            
            //provide the initial solution to the solver
            void initial_solution(Eigen::VectorXd& x0){
                //using the original mesh
                x0.conservativeResize(3*VOrig.rows()+EV.rows());
                for (int i=0;i<VOrig.rows();i++)
                    x0.segment(3*i,3)<<VOrig.row(i).transpose();
                x0.tail(EV.rows()).setZero();
            }
            
            void pre_iteration(const Eigen::VectorXd& prevx){}
            bool post_iteration(const Eigen::VectorXd& x){return false;  /*never stop after an iteration*/}
            
            
            //updating the energy vector for a given current solution
            void update_energy(const Eigen::VectorXd& x){
                
                using namespace std;
                using namespace Eigen;
                
                MatrixXd currV(VOrig.rows(),3);
                for (int i=0;i<VOrig.rows();i++)
                    currV.row(i)<<x.segment(3*i,3).transpose();
                
                if (oType==VERTEX_OFFSET){
                    EVec=(VOrig-currV).rowwise().squaredNorm().array()-d*d;
                }
                
                for (int i=0;i<EVec.size();i++)
                    if (isnan(EVec(i)))
                        cout<<"nan in EVec("<<i<<")"<<endl;
            }
            
            
            //update the jacobian values for a given current solution
            void update_jacobian(const Eigen::VectorXd& x){
                using namespace std;
                using namespace Eigen;
                
                MatrixXd currV(VOrig.rows(),3);
                for (int i=0;i<VOrig.rows();i++)
                    currV.row(i)<<x.segment(3*i,3).transpose();
                
                
                //Energy Jacobian
                if (oType==VERTEX_OFFSET) {
                    for (int i=0;i<VOrig.rows();i++){
                        for (int j=0;j<3;j++){
                            JEVals(3*i+j)=2*VOrig(i,j);
                        }
                    }
                }
                
                for (int i=0;i<JEVals.size();i++)
                    if (isnan(JEVals(i)))
                        cout<<"nan in JEVals("<<i<<")"<<endl;
                
                //Constraint Jacobian is constant
            }
            
            void update_constraints(const Eigen::VectorXd& x){
                CVec<<offsetConstMat*x;
            }

            bool post_optimization(const Eigen::VectorXd& x){
                fullSolution.conservativeResize(VOrig.rows(),3);
                for (int i=0;i<VOrig.rows();i++)
                    fullSolution.row(i)<<x.segment(3*i,3).transpose();
                
                return true;  //this traits doesn't have any more stop requirements
            }
            
            OffsetMeshTraits(){}
            ~OffsetMeshTraits(){}
        };
        
        
    } }


#endif