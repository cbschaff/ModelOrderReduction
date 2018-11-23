import Sofa
import os

from numpy import add,subtract,multiply
from splib.numerics import *

from controller import SofiaLegController

path = os.path.dirname(os.path.abspath(__file__))
meshPath = path + '/mesh/'

def TRSinOrigin(positions,modelPosition,translation,rotation,scale=[1.0,1.0,1.0]):
    posOrigin = subtract(positions , modelPosition)
    if any(isinstance(el, list) for el in positions):
        posOriginTRS = transformPositions(posOrigin,translation,rotation,scale=scale)
    else:
        posOriginTRS = transformPosition(posOrigin,TRS_to_matrix(translation,eulerRotation=rotation,scale=scale))
    return add(posOriginTRS,modelPosition).tolist()
    
def newBox(positions,modelPosition,translation,rotation,offset,scale=[1.0,1.0,1.0]):
    pos = TRSinOrigin(positions,modelPosition,translation,rotation,scale)
    offset =transformPositions([offset],eulerRotation=rotation,scale=scale)[0]
    return add(pos,offset).tolist()


def SofiaLeg(
              attachedTo=None,
              name="SofiaLeg",
              volumeMeshFileName='sofia_leg.vtu',
              rotation=[0.0, 0.0, 0.0],
              translation=[0.0, 0.0, 0.0],
              scale=[1.0, 1.0, 1.0],
              surfaceMeshFileName='sofia_leg.stl',
              surfaceColor=[1.0, 1.0, 1.0],
              poissonRatio=0.45,
              youngModulus=300,
              totalMass=0.01,
              controller=None):
    """
    Object with an elastic deformation law.

    Args:

        attachedTo (Sofa.Node): Where the node is created;

        name (str) : name of the Sofa.Node it will 

        surfaceMeshFileName (str): Filepath to a surface mesh (STL, OBJ). 
                                   If missing there is no visual properties to this object.

        surfaceColor (vec3f):  The default color used for the rendering of the object.

        rotation (vec3f):   Apply a 3D rotation to the object in Euler angles.

        translation (vec3f):   Apply a 3D translation to the object.

        scale (vec3f): Apply a 3D scale to the object.

        poissonRatio (float):  The poisson parameter.

        youngModulus (float):  The young modulus.

        totalMass (float):   The mass is distributed according to the geometry of the object.
    """
    leg = attachedTo.createChild(name)
    leg.createObject('EulerImplicit' , firstOrder = '0', name = 'odesolver')
    leg.createObject('SparseLDLSolver' , name = 'preconditioner')


    leg.createObject('MeshVTKLoader' ,name = 'loader', scale3d = scale, translation = translation, rotation = rotation, filename = meshPath+volumeMeshFileName)
    leg.createObject('TetrahedronSetTopologyContainer' ,name = 'container',  position = '@loader.position',tetrahedra = '@loader.tetrahedra', checkConnexity = '1', createTriangleArray = '1')
    leg.createObject('MechanicalObject' , name = 'tetras', showIndices = 'false', showIndicesScale = '4e-5', template = 'Vec3d', position = '@loader.position')
    leg.createObject('UniformMass' , totalMass = totalMass)
    leg.createObject('TetrahedronFEMForceField' , youngModulus = youngModulus, poissonRatio = poissonRatio)
    leg.createObject('LinearSolverConstraintCorrection', solverName='preconditioner')
    #To fix the Top part of the leg
    leg.createObject('BoxROI' , name= 'boxROITop' , orientedBox= newBox([[-12.0, 53.0, 0], [12.0, 53.0, 0], [12.0, 64.0, 0]] , [0.0, 0.0, 0.0],translation,rotation,[0, 0, 0.0],scale) + multiply(scale[2],[16.0]).tolist(),drawBoxes=False)
    #leg.createObject('RestShapeSpringsForceField' , name = 'fixedTopForceField', points = '@boxROITop.indices', stiffness = '1e12')
    
    #Box to add collisions only on the tip of the leg
    leg.createObject('BoxROI', name='boxROICollision', orientedBox= newBox(
                                                                    [[-25.0, -41.0, 0],[25.0, -42, 0],[25.0, -39, 0]], [0.0, 0.0, 0.0],
                                                                    translation,rotation,[0, 0, -7.0],scale) + multiply(scale[2],[2.0]).tolist()
                                                                +newBox(
                                                                    [[-25.0, -42, 0],[25.0, -42, 0],[25.0, -39, 0]], [0.0, 0.0, 0.0],
                                                                    translation,rotation,[0, 0, 7.0],scale) + multiply(scale[2],[2.0]).tolist(),
                                                    drawPoints='0', computeEdges='0',computeTriangles='0', computeTetrahedra='0',
                                                    computeHexahedra='0', computeQuad='0',drawSize=5, drawBoxes=False)

    #To Actuate our leg we select some elements in the middle of our leg, add a Spring to them, then add an external_rest_shape that will allow us 
    leg.createObject('BoxROI' , name= 'boxROIMiddle' , orientedBox= newBox([[-2.5, -8.5, 0], [2.5, -8.5, 0], [2.5, -3.5, 0]] , [0.0, 0.0, 0.0],translation,rotation,[0, 0, 0.0],scale) + multiply(scale[2],[18.0]).tolist(),drawBoxes=False)
    #leg.createObject('RestShapeSpringsForceField' , external_points = [0, 1, 2], points = '@boxROIMiddle.indices', name = 'actuatorSpring', stiffness = '1e12', external_rest_shape = '@../'+name+'_actuator/actuatorState')

    SofiaLeg_actuator = attachedTo.createChild(name+'_actuator')
    SofiaLeg_actuator.createObject('MechanicalObject' , name = 'actuatorState', position = '@../'+name+'/boxROIMiddle.pointsInROI', template = 'Vec3d', showObject = False)

    ## Visualization
    if surfaceMeshFileName:
        visu = leg.createChild('Visual')

        meshType = surfaceMeshFileName.split('.')[-1]
        if meshType == 'stl':
            visu.createObject(  'MeshSTLLoader', name= 'loader', filename=path+'/mesh/'+surfaceMeshFileName)
        elif meshType == 'obj':
            visu.createObject(  'MeshObjLoader', name= 'loader', filename=path+'/mesh/'+surfaceMeshFileName)

        visu.createObject(  'OglModel',
                            src='@loader',
                            template='ExtVec3f',
                            color=surfaceColor,
                            rotation= rotation,
                            translation = translation,
                            scale3d = scale)

        visu.createObject('BarycentricMapping')

    if controller != None:
        myController = SofiaLegController(SofiaLeg_actuator)
        myController.init(**controller)

        return leg , myController

    return leg

def createScene(rootNode):
    from stlib.scene import MainHeader
    surfaceMeshFileName = 'sofia_leg.stl'

    MainHeader(rootNode,plugins=["SofaPython","SoftRobots","ModelOrderReduction"],
                        dt=0.01,
                        gravity=[0, -9810, 0])

    SofiaLeg(rootNode,
                    name="SofiaLeg_blue_1", 
                    rotation=[0, 0.0, 0.0],
                    translation=[0, 0.0, 0.0],
                    surfaceColor=[0.0, 0.0, 1, 0.5],
                    controller={'offset':40},
                    surfaceMeshFileName=surfaceMeshFileName)

    SofiaLeg(rootNode,
                    name="SofiaLeg_blue_2", 
                    rotation=[0, 0.0, 0.0],
                    translation=[0, 0.0, -40.0],
                    surfaceColor=[0.0, 1, 0, 0.5],
                    controller={},
                    surfaceMeshFileName=surfaceMeshFileName)

    return rootNode