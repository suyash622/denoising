#include "Mesh.h"
#include "MeshIO.h"
#include <Eigen/SparseLU>

Mesh::Mesh()
{
    
}

Mesh::Mesh(const Mesh& mesh)
{
    *this = mesh;
}

bool Mesh::read(const std::string& fileName)
{
    std::ifstream in(fileName.c_str());

    if (!in.is_open()) {
        std::cerr << "Error: Could not open file for reading" << std::endl;
        return false;
    }
    
    bool readSuccessful = false;
    if ((readSuccessful = MeshIO::read(in, *this))) {
        normalize();
    }
    
    return readSuccessful;
}

bool Mesh::write(const std::string& fileName) const
{
    std::ofstream out(fileName.c_str());
    
    if (!out.is_open()) {
        std::cerr << "Error: Could not open file for writing" << std::endl;
        return false;
    }
    
    MeshIO::write(out, *this);
    
    return false;
}

void Mesh::buildFlowOperator(Eigen::SparseMatrix<double>& A, const double h) const
{
    A.resize((int)vertices.size(), (int)vertices.size());
    
    for (VertexCIter v = vertices.begin(); v != vertices.end(); v++) {
        
        HalfEdgeCIter he = v->he;
        double dualArea = v->dualArea();
        double sumCoefficients = 0.0;

        do {
            double coefficient = -(he->cotan() + he->flip->cotan());
            
            A.insert(v->index, he->flip->vertex->index) = coefficient;
            sumCoefficients += coefficient;
            
            he = he->flip->next;
        } while (he != v->he);
        
        A.insert(v->index, v->index) = 2 * dualArea / h - sumCoefficients;
    }
    
    A.makeCompressed();
}

void Mesh::computeMeanCurvatureFlow(const double h)
{
    // solve (Id - hL) v(t+h) = v(t)
    
    int v = (int)vertices.size();
    
    Eigen::VectorXd fx(v);
    Eigen::VectorXd fy(v);
    Eigen::VectorXd fz(v);
    
    for (VertexCIter v = vertices.begin(); v != vertices.end(); v++) {
        fx(v->index) = 2 * v->dualArea() * v->position.x() / h;
        fy(v->index) = 2 * v->dualArea() * v->position.y() / h;
        fz(v->index) = 2 * v->dualArea() * v->position.z() / h;
    }
    
    Eigen::SparseMatrix<double> A;
    buildFlowOperator(A, h);
    
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(A);
    
    fx = solver.solve(fx);
    fy = solver.solve(fy);
    fz = solver.solve(fz);
    
    for (VertexIter v = vertices.begin(); v != vertices.end(); v++) {
        v->position.x() = fx(v->index);
        v->position.y() = fy(v->index);
        v->position.z() = fz(v->index);
    }
}

void Mesh::normalize()
{
    // compute center of mass
    Eigen::Vector3d cm = Eigen::Vector3d::Zero();
    for (VertexCIter v = vertices.begin(); v != vertices.end(); v++) {
        cm += v->position;
    }
    cm /= (double)vertices.size();
    
    // translate to origin
    for (VertexIter v = vertices.begin(); v != vertices.end(); v++) {
        v->position -= cm;
    }
    
    // determine radius
    double rMax = 0;
    for (VertexCIter v = vertices.begin(); v != vertices.end(); v++) {
        rMax = std::max(rMax, v->position.norm());
    }
    
    // rescale to unit sphere
    for (VertexIter v = vertices.begin(); v != vertices.end(); v++) {
        v->position /= rMax;
    }
}
