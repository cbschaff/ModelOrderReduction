/******************************************************************************
*            Model Order Reduction plugin for SOFA                            *
*                         version 1.0                                         *
*                       Copyright © Inria                                     *
*                       All rights reserved                                   *
*                       2018                                                  *
*                                                                             *
* This software is under the GNU General Public License v2 (GPLv2)            *
*            https://www.gnu.org/licenses/licenses.en.html                    *
*                                                                             *
*                                                                             *
*                                                                             *
* Authors: Olivier Goury, Felix Vanneste                                      *
*                                                                             *
* Contact information: https://project.inria.fr/modelorderreduction/contact   *
******************************************************************************/
#define SOFA_COMPONENT_FORCEFIELD_HYPERREDUCEDRESTSHAPESPRINGSFORCEFIELD_CPP

#include "HyperReducedRestShapeSpringsForceField.inl"

#include <sofa/core/visual/DrawTool.h>
#include <sofa/core/ObjectFactory.h>

namespace sofa
{

namespace component
{

namespace forcefield
{

using namespace sofa::defaulttype;


SOFA_DECL_CLASS(HyperReducedRestShapeSpringsForceField)

///////////// SPECIALIZATION FOR RIGID TYPES //////////////


#ifndef SOFA_FLOAT

template<>
void HyperReducedRestShapeSpringsForceField<Rigid3dTypes>::addForce(const core::MechanicalParams* /* mparams */, DataVecDeriv& f, const DataVecCoord& x, const DataVecDeriv& /* v */)
{
    sofa::helper::WriteAccessor< DataVecDeriv > f1 = f;
    sofa::helper::ReadAccessor< DataVecCoord > p1 = x;

    sofa::helper::ReadAccessor< DataVecCoord > p0 = *getExtPosition();

    f1.resize(p1.size());

    if (recompute_indices.getValue())
    {
        recomputeIndices();
    }

    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();

    for (unsigned int i = 0; i < m_indices.size(); i++)
    {
        const unsigned int index = m_indices[i];
        unsigned int ext_index = m_indices[i];
        if(useRestMState)
            ext_index= m_ext_indices[i];

        // translation
        if (i >= m_pivots.size())
        {
            Vec3d dx = p1[index].getCenter() - p0[ext_index].getCenter();
            getVCenter(f1[index]) -=  dx * (i < k.size() ? k[i] : k[0]) ;
        }
        else
        {
            CPos localPivot = p0[ext_index].getOrientation().inverseRotate(m_pivots[i] - p0[ext_index].getCenter());
            CPos rotatedPivot = p1[index].getOrientation().rotate(localPivot);
            CPos pivot2 = p1[index].getCenter() + rotatedPivot;
            CPos dx = pivot2 - m_pivots[i];
            getVCenter(f1[index]) -= dx * (i < k.size() ? k[i] : k[0]) ;
        }

        // rotation
        Quatd dq = p1[index].getOrientation() * p0[ext_index].getOrientation().inverse();
        Vec3d dir;
        double angle=0;
        dq.normalize();

        if (dq[3] < 0)
        {
            dq = dq * -1.0;
        }

        if (dq[3] < 0.999999999999999)
            dq.quatToAxis(dir, angle);

        getVOrientation(f1[index]) -= dir * angle * (i < k_a.size() ? k_a[i] : k_a[0]);
    }
}


template<>
void HyperReducedRestShapeSpringsForceField<Rigid3dTypes>::addDForce(const core::MechanicalParams* mparams, DataVecDeriv& df, const DataVecDeriv& dx)
{
    sofa::helper::WriteAccessor< DataVecDeriv > df1 = df;
    sofa::helper::ReadAccessor< DataVecDeriv > dx1 = dx;

    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();
    Real kFactor = (Real)mparams->kFactorIncludingRayleighDamping(this->rayleighStiffness.getValue());

    unsigned int curIndex = 0;

    for (unsigned int i=0; i<m_indices.size(); i++)
    {
        curIndex = m_indices[i];
        getVCenter(df1[curIndex])	 -=  getVCenter(dx1[curIndex]) * ( (i < k.size()) ? k[i] : k[0] ) * kFactor ;
        getVOrientation(df1[curIndex]) -=  getVOrientation(dx1[curIndex]) * (i < k_a.size() ? k_a[i] : k_a[0]) * kFactor ;
    }
}


template<>
void HyperReducedRestShapeSpringsForceField<Rigid3dTypes>::addKToMatrix(const core::MechanicalParams* mparams, const sofa::core::behavior::MultiMatrixAccessor* matrix )
{
    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();
    const int N = 6;
    sofa::core::behavior::MultiMatrixAccessor::MatrixRef mref = matrix->getMatrix(this->mstate);
    sofa::defaulttype::BaseMatrix* mat = mref.matrix;
    unsigned int offset = mref.offset;
    Real kFact = (Real)mparams->kFactorIncludingRayleighDamping(this->rayleighStiffness.getValue());

    unsigned int curIndex = 0;

    for (unsigned int index = 0; index < m_indices.size(); index++)
    {
        curIndex = m_indices[index];

        // translation
        for(int i = 0; i < 3; i++)
        {
            mat->add(offset + N * curIndex + i, offset + N * curIndex + i, -kFact * (index < k.size() ? k[index] : k[0]));
        }

        // rotation
        for(int i = 3; i < 6; i++)
        {
            mat->add(offset + N * curIndex + i, offset + N * curIndex + i, -kFact * (index < k_a.size() ? k_a[index] : k_a[0]));
        }
    }
}

template<>
void HyperReducedRestShapeSpringsForceField<Rigid3dTypes>::draw(const core::visual::VisualParams* vparams)
{
    if (!vparams->displayFlags().getShowForceFields() || !drawSpring.getValue())
        return;  /// \todo put this in the parent class

    vparams->drawTool()->saveLastState();
    vparams->drawTool()->setLightingEnabled(false);

    sofa::helper::ReadAccessor< DataVecCoord > p0 = *getExtPosition();
    sofa::helper::ReadAccessor< DataVecCoord > p = this->mstate->read(core::VecCoordId::position());

    sofa::helper::vector< Vector3 > vertices;

    for (unsigned int i=0; i<m_indices.size(); i++)
    {
        const unsigned int index = m_indices[i];

        vertices.push_back(p[index].getCenter());

        if(useRestMState)
        {
            const unsigned int ext_index = m_ext_indices[i];
            vertices.push_back(p0[ext_index].getCenter());
        }
        else
        {
            vertices.push_back(p0[index].getCenter());
        }
    }
    vparams->drawTool()->drawLines(vertices,5, Vec4f(springColor.getValue()));
    vparams->drawTool()->restoreLastState();
}


#endif // SOFA_FLOAT

#ifndef SOFA_DOUBLE

template<>
void HyperReducedRestShapeSpringsForceField<Rigid3fTypes>::addForce(const core::MechanicalParams* /* mparams */, DataVecDeriv& f, const DataVecCoord& x, const DataVecDeriv& /* v */)
{
    sofa::helper::WriteAccessor< DataVecDeriv > f1 = f;
    sofa::helper::ReadAccessor< DataVecCoord > p1 = x;

    sofa::helper::ReadAccessor< DataVecCoord > p0 = *getExtPosition();

    f1.resize(p1.size());

    if (recompute_indices.getValue())
    {
        recomputeIndices();
    }

    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();

    for (unsigned int i=0; i<m_indices.size(); i++)
    {
        const unsigned int index = m_indices[i];
        const unsigned int ext_index = m_ext_indices[i];

        // translation
        Vec3f dx = p1[index].getCenter() - p0[ext_index].getCenter();
        getVCenter(f1[index]) -=  dx * (i < k.size() ? k[i] : k[0]) ;

        // rotation
        Quatf dq = p1[index].getOrientation() * p0[ext_index].getOrientation().inverse();
        Vec3f dir;
        Real angle=0;
        dq.normalize();
        if (dq[3] < 0.999999999999999)
            dq.quatToAxis(dir, angle);
        dq.quatToAxis(dir, angle);

        getVOrientation(f1[index]) -= dir * angle * (i < k_a.size() ? k_a[i] : k_a[0]) ;
    }
}


template<>
void HyperReducedRestShapeSpringsForceField<Rigid3fTypes>::addDForce(const core::MechanicalParams* mparams, DataVecDeriv& df, const DataVecDeriv& dx)
{
    sofa::helper::WriteAccessor< DataVecDeriv > df1 = df;
    sofa::helper::ReadAccessor< DataVecDeriv > dx1 = dx;
    Real kFactor = (Real)mparams->kFactorIncludingRayleighDamping(this->rayleighStiffness.getValue());

    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();

    unsigned int curIndex = 0;

    for (unsigned int i=0; i<m_indices.size(); i++)
    {
        curIndex = m_indices[i];  // Fix by FF, just a guess
        getVCenter(df1[curIndex])      -=  getVCenter(dx1[curIndex])      * (i < k.size() ? k[i] : k[0])   * kFactor ;
        getVOrientation(df1[curIndex]) -=  getVOrientation(dx1[curIndex]) * (i < k_a.size() ? k_a[i] : k_a[0]) * kFactor ;
    }
}


template<>
void HyperReducedRestShapeSpringsForceField<Rigid3fTypes>::addKToMatrix(const core::MechanicalParams* mparams, const sofa::core::behavior::MultiMatrixAccessor* matrix )
{
    const VecReal& k = stiffness.getValue();
    const VecReal& k_a = angularStiffness.getValue();
    const int N = 6;

    sofa::core::behavior::MultiMatrixAccessor::MatrixRef mref = matrix->getMatrix(this->mstate);
    sofa::defaulttype::BaseMatrix* mat = mref.matrix;
    unsigned int offset = mref.offset;
    Real kFact = (Real)mparams->kFactorIncludingRayleighDamping(this->rayleighStiffness.getValue());

    unsigned int curIndex = 0;


    for (unsigned int index = 0; index < m_indices.size(); index++)
    {
        curIndex = m_indices[index];

        // translation
        for(int i = 0; i < 3; i++)
        {
            mat->add(offset + N * curIndex + i, offset + N * curIndex + i, kFact * (index < k.size() ? k[index] : k[0]));
        }

        // rotation
        for(int i = 3; i < 6; i++)
        {
            mat->add(offset + N * curIndex + i, offset + N * curIndex + i, kFact * (index < k_a.size() ? k_a[index] : k_a[0]));
        }
    }
}

template<>
void HyperReducedRestShapeSpringsForceField<Rigid3fTypes>::draw(const core::visual::VisualParams* vparams)
{
    if (!vparams->displayFlags().getShowForceFields() || !drawSpring.getValue())
        return;  /// \todo put this in the parent class

    vparams->drawTool()->saveLastState();
    vparams->drawTool()->setLightingEnabled(false);

    sofa::helper::ReadAccessor< DataVecCoord > p0 = *getExtPosition();
    sofa::helper::ReadAccessor< DataVecCoord > p = this->mstate->read(core::VecCoordId::position());

    sofa::helper::vector<sofa::defaulttype::Vector3> vertices;

    for (unsigned int i=0; i<m_indices.size(); i++)
    {
        const unsigned int index = m_indices[i];

        sofa::defaulttype::Vector3 v0(p[index].getCenter()[0],
                                      p[index].getCenter()[1],
                                      p[index].getCenter()[2]);
        unsigned int tempIndex = (useRestMState) ? m_ext_indices[i] : index;

        sofa::defaulttype::Vector3 v1(p0[tempIndex].getCenter()[0],
                                      p0[tempIndex].getCenter()[1],
                                      p0[tempIndex].getCenter()[2]);


        vertices.push_back(v0);
        vertices.push_back(v1);
    }

    vparams->drawTool()->drawLines(vertices,5,springColor.getValue());

    vparams->drawTool()->restoreLastState();
}

#endif // SOFA_DOUBLE


int HyperReducedRestShapeSpringsForceFieldClass = core::RegisterObject("Simple elastic springs applied to given degrees of freedom between their current and rest shape position")
#ifndef SOFA_FLOAT
        .add< HyperReducedRestShapeSpringsForceField<Vec3dTypes> >()
#endif
#ifndef SOFA_DOUBLE
        .add< HyperReducedRestShapeSpringsForceField<Vec3fTypes> >()
#endif
        ;

#ifndef SOFA_FLOAT
template class SOFA_DEFORMABLE_API HyperReducedRestShapeSpringsForceField<Vec3dTypes>;
#endif
#ifndef SOFA_DOUBLE
template class SOFA_DEFORMABLE_API HyperReducedRestShapeSpringsForceField<Vec3fTypes>;
#endif

} // namespace forcefield

} // namespace component

} // namespace sofa