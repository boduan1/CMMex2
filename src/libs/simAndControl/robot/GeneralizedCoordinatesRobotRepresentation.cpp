#include <robot/GeneralizedCoordinatesRobotRepresentation.h>
#include <robot/RBJoint.h>
#include <utils/utils.h>

namespace crl {

GeneralizedCoordinatesRobotRepresentation::
    GeneralizedCoordinatesRobotRepresentation(Robot *a) {
    robot = a;
    resize(q, getDOFCount());
    syncGeneralizedCoordinatesWithRobotState();
}

//returns the total number of degrees of freedom in the reduced representation
int GeneralizedCoordinatesRobotRepresentation::getDOFCount() const {
    //we will have 3 for root position, 3 for root orientation, and then 1 for each joint,
    //assuming they're all hinge joints, which is what this data structure is designed for
    return 6 + (int)robot->jointList.size();
}

//returns the index of the generalized coordinate corresponding to this joint
int GeneralizedCoordinatesRobotRepresentation::getQIdxForJoint(
    RBJoint *joint) const {
    //the root of the robot has a nullptr joint, and as such this should correspond to the first qIdx of the root DOFs
    if (joint == NULL) return 5;
    return 6 + joint->jIndex;
}

//returns the index of the generalized coordinate corresponding to the index of this joint
int GeneralizedCoordinatesRobotRepresentation::getQIdxForJointIdx(
    int jIdx) const {
    return 6 + jIdx;
}

//returns a pointer to the joint corresponding to this generalized coordinate index.
//If the index corresponds to a root DOF, this method will return NULL.
RBJoint *GeneralizedCoordinatesRobotRepresentation::getJointForQIdx(
    int qIdx) const {
    if (qIdx < 6) return NULL;
    return robot->jointList[qIdx - 6];
}

/**
* In the tree-like hierarchy of joints/dofs, this method returns the parent index of the
* dof corresponding to qIdx
*/
int GeneralizedCoordinatesRobotRepresentation::getParentQIdxOf(int qIdx) {
    if (qIdx < 6) return qIdx - 1;
    return getQIdxForJoint(getJointForQIdx(qIdx)->parent->pJoint);
}

// updates q and qDot given current state of robot
void GeneralizedCoordinatesRobotRepresentation::
    syncGeneralizedCoordinatesWithRobotState() {
    // write out the position of the root...
    RobotState state(robot);
    P3D pos = state.getPosition();
    Quaternion orientation = state.getOrientation();

    q[0] = pos.x;
    q[1] = pos.y;
    q[2] = pos.z;

    // Root
    computeEulerAnglesFromQuaternion(orientation, getQAxis(5), getQAxis(4),
                                     getQAxis(3), q[5], q[4], q[3]);

    // Now go through each joint, and decompose it as appropriate...
    for (uint i = 0; i < robot->jointList.size(); i++) {
        int qIdx = getQIdxForJointIdx(i);
        // Only 1-dof hinge joints
        computeRotationAngleFromQuaternion(state.getJointRelativeOrientation(i),
                                           getQAxis(qIdx), q[qIdx]);
    }
}

// returns the axis corresponding to the indexed generalized coordinate,
// expressed in local coordinates
V3D GeneralizedCoordinatesRobotRepresentation::getQAxis(int qIndex) const {
    if (qIndex >= 0 || qIndex < 6) {
        // the first three are the translational dofs of the body
        if (qIndex == 0) return V3D(1, 0, 0);
        if (qIndex == 1) return V3D(0, 1, 0);
        if (qIndex == 2) return V3D(0, 0, 1);
        if (qIndex == 3) return RBGlobals::worldUp;  // y - yaw
        if (qIndex == 4)
            return RBGlobals::worldUp.cross(robot->forward);  // x - pitch
        if (qIndex == 5) return robot->forward;               // z - roll
    }

    return getJointForQIdx(qIndex)->rotationAxis;
}

void GeneralizedCoordinatesRobotRepresentation::
    syncRobotStateWithGeneralizedCoordinates() {
    RobotState rs(robot);
    getReducedRobotState(rs);
    robot->setState(&rs);
}

// given the current state of the generalized representation, output the reduced
// state of the robot
void GeneralizedCoordinatesRobotRepresentation::getReducedRobotState(
    RobotState &state) {
    // set the position, velocity, rotation and angular velocity for the root
    state.setPosition(P3D(0, 0, 0) + getQAxis(0) * q[0] + getQAxis(1) * q[1] +
                      getQAxis(2) * q[2]);
    state.setOrientation(getOrientationFor(robot->root));

    for (uint i = 0; i < robot->jointList.size(); i++) {
        int qIdx = getQIdxForJointIdx(i);
        Quaternion jointOrientation =
            getRotationQuaternion(q[qIdx], getQAxis(qIdx));

        state.setJointRelativeOrientation(jointOrientation, i);
    }
    // and done...
}

// sets the current q values
void GeneralizedCoordinatesRobotRepresentation::setQ(const dVector &qNew) {
    assert(q.size() == qNew.size());
    // NOTE: we don't update the angular velocities. The assumption is that the
    // correct behavior is that the joint relative angular velocities don't
    // change, although the world relative values of the rotations do
    q = qNew;
}

// gets the current q values
void GeneralizedCoordinatesRobotRepresentation::getQ(dVector &q_copy) {
    q_copy = q;
}

void GeneralizedCoordinatesRobotRepresentation::getQFromReducedState(
    const RobotState &rs, dVector &q_copy) {
    dVector q_old = q;

    RobotState oldState(robot);
    robot->setState((RobotState *)&rs);
    syncGeneralizedCoordinatesWithRobotState();
    getQ(q_copy);
    robot->setState(&oldState);

    setQ(q_old);
}

/**
    pLocal is expressed in the coordinate frame of the link that pivots about DOF qIdx.
    This method returns the point in the coordinate frame of the parent of qIdx after
    the DOF rotation has been applied.
*/
P3D GeneralizedCoordinatesRobotRepresentation::
    getCoordsInParentQIdxFrameAfterRotation(int qIndex, const P3D &pLocal) {
    // if qIndex <= 2, this q is a component of position of the base. 
    if (qIndex <= 2) return pLocal;

    // TODO: Ex.1 Forward Kinematics
    // this is a subfunction for getWorldCoordinates() and compute_dpdq()
    // return the point in the coordinate frame of the parent of qIdx after
    // the DOF rotation has been applied.
    // 
    // Hint:
    // - use rotateVec(const V3D &v, double alpha, const V3D &axis) to get a vector 
    // rotated around axis by angle alpha.
    
    // TODO: implement your logic here.
    
    V3D axis = getQAxis(qIndex);
    double alpha;
    P3D pJoint = P3D();
    P3D cJoint = P3D();
    if(qIndex>5){
        //alpha = getJointForQIdx(qIndex)->getCurrentJointAngle();
        alpha = q[qIndex];
        pJoint = getJointForQIdx(qIndex)->pJPos;
        cJoint = getJointForQIdx(qIndex)->cJPos;
    }else{
        alpha = q[qIndex];
    }
    V3D v = V3D(cJoint,pLocal);
    return pJoint+rotateVec( v, alpha, axis);
    //return P3D();
}

// returns the world coordinates for point p, which is specified in the local
// coordinates of rb (relative to its COM): p(q)
P3D GeneralizedCoordinatesRobotRepresentation::getWorldCoordinates(const P3D &p,
                                                                   RB *rb) {
    // TODO: Ex.1 Forward Kinematics
    // implement subfunction getCoordsInParentQIdxFrameAfterRotation() first.
    //
    // Hint: you may want to use the following functions
    // - getQIdxForJoint()
    // - getParentQIdxOf()
    // - getCoordsInParentQIdxFrameAfterRotation() 

    P3D pInWorld;

    // TODO: implement your logic here.
    //
    //
    
    int qIndex = getQIdxForJoint(rb->pJoint);
    P3D pLocal = p;
    //pLocal += (rb->state).pos;

    while(qIndex>=6){
        pLocal = getCoordsInParentQIdxFrameAfterRotation(qIndex, pLocal);
        qIndex = getParentQIdxOf(qIndex);
    }
    for(int i=3;i<6;i++){
        pLocal = getCoordsInParentQIdxFrameAfterRotation(i, pLocal);
    }
    P3D pBase = P3D(q[0],q[1],q[2]);
    pInWorld = pBase + pLocal;
    return pInWorld;
}

// returns the global orientation associated with a specific dof q...
Quaternion GeneralizedCoordinatesRobotRepresentation::getWorldRotationForQ(
    int qIndex) {
    Quaternion qRes = Quaternion::Identity();
    // 2 here is the index of the first translational DOF of the root -- these
    // dofs do not contribute to the orientation of the rigid bodies...
    while (qIndex > 2) {
        qRes = getRelOrientationForQ(qIndex) * qRes;
        qIndex = getParentQIdxOf(qIndex);
    }
    return qRes;
}

Quaternion GeneralizedCoordinatesRobotRepresentation::getRelOrientationForQ(
    int qIndex) {
    if (qIndex < 3) return Quaternion::Identity();
    return getRotationQuaternion(q[qIndex], getQAxis(qIndex));
}

// this is a somewhat slow function to use if we must iterate through multiple
// rigid bodies...
V3D GeneralizedCoordinatesRobotRepresentation::getWorldCoordsAxisForQ(
    int qIndex) {
    if (qIndex < 3) return getQAxis(qIndex);
    return getWorldRotationForQ(qIndex) * getQAxis(qIndex);
}

// returns the world-relative orientation for rb
Quaternion GeneralizedCoordinatesRobotRepresentation::getOrientationFor(
    RB *rb) {
    int qIndex = getQIdxForJoint(rb->pJoint);
    return getWorldRotationForQ(qIndex);
}

// computes the jacobian dp/dq that tells you how the world coordinates of p
// change with q. p is expressed in the local coordinates of rb
void GeneralizedCoordinatesRobotRepresentation::compute_dpdq(const P3D &p,
                                                             RB *rb,
                                                             Matrix &dpdq) {
    resize(dpdq, 3, (int)q.size());

    // TODO: Ex.4 Analytic Jacobian
    //
    // Hint: you may want to use the following functions
    // - getQIdxForJoint()
    // - getCoordsInParentQIdxFrameAfterRotation()
    // - getQAxis()
    // - rotateVec()
    // - getParentQIdxOf()

    // TODO: your implementation should be here
    resize(dpdq, 3, (int)q.size());
    for (int i = 0; i < q.size(); i++){
        dpdq(0, i) = 0;
        dpdq(1, i) = 0;
        dpdq(2, i) = 0;
    }
    for(int i=0;i<3;i++){
        dpdq(i, i) = 1;
    }

    std::vector<int> joint;
    int qIndex = getQIdxForJoint(rb->pJoint);
    while(qIndex>=6){
        joint.push_back(qIndex);
        qIndex = getParentQIdxOf(qIndex);
    }
    joint.push_back(3);
    joint.push_back(4);
    joint.push_back(5);
    int l = joint.size();

    /*std::vector<V3D> N(q.size());
    V3D axis_w = getQAxis(5);
    N[5] = axis_w;
    for (int i=l-2;i>=0;i--) {
        int idx = joint[i];
        V3D axis2 = getQAxis(idx);
        double alpha = q(idx);
        V3D axis_w = rotateVec(axis_w, alpha, axis2);
        N[idx] = axis_w;
    }*/
    std::vector<P3D> A(q.size());
    std::vector<P3D> P(q.size());
    for (int i = 0; i < q.size(); i++){
        P[i] = P3D();
    }
    for (int i = 6; i < q.size(); i++){
        P3D cJoint = getJointForQIdx(i)->cJPos;
        P[i] = cJoint;
    }
    
    for (int i = 0; i < q.size(); i++){
        A[i] = P[i] + getQAxis(i);
    }
    P3D p_w = p;
    P3D p_w2 = getWorldCoordinates(p,rb);
    for (int i=0;i<l;i++) {
        int idx = joint[i];
        p_w = getCoordsInParentQIdxFrameAfterRotation(idx, p_w);
        for(int j=0;j<=i;j++){
            int idx2 = joint[j];
            P[idx2] = getCoordsInParentQIdxFrameAfterRotation(idx, P[idx2]);
            A[idx2] = getCoordsInParentQIdxFrameAfterRotation(idx, A[idx2]);
        }
    }
    /*for(auto i:joint){
        std::cout<<i<<' ';
    }std::cout<<std::endl;
    std::cout<<p_w.x+q[0]<<' '<<p_w.y+q[1]<<' '<<p_w.z+q[2]<<std::endl;
    std::cout<<p_w2.x<<' '<<p_w2.y<<' '<<p_w2.z<<std::endl;*/
    for (int i :joint) {
        V3D D = V3D(P[i],p_w);
        //V3D axis = N[i];
        V3D axis = V3D(P[i],A[i]);
        V3D dpdqi = axis.cross(D);
        dpdq(0, i) = dpdqi[0];
        dpdq(1, i) = dpdqi[1];
        dpdq(2, i) = dpdqi[2];
    }
}

// estimates the linear jacobian dp/dq using finite differences
void GeneralizedCoordinatesRobotRepresentation::estimate_linear_jacobian(
    const P3D &p, RB *rb, Matrix &dpdq) {
    resize(dpdq, 3, (int)q.size());

    for (int i = 0; i < q.size(); i++) {
        double val = q[i];
        double h = 0.0001;

        // TODO: Ex. 2-1 Inverse Kinematics - Jacobian by Finite Difference
        // compute Jacobian matrix dpdq_i by FD and fill dpdq
        q[i] = val + h;
        P3D p_p = getWorldCoordinates(p,rb);  // TODO: fix this: p(qi + h);

        q[i] = val - h;
        P3D p_m = getWorldCoordinates(p,rb);  // TODO: fix this: p(qi - h)

        V3D dpdq_i = V3D(p_m,p_p)/(2*h);  // TODO: fix this: compute derivative dp(q)/dqi

        // set Jacobian matrix components
        dpdq(0, i) = dpdq_i[0];
        dpdq(1, i) = dpdq_i[1];
        dpdq(2, i) = dpdq_i[2];

        // finally, we don't want to change q[i] value. back to original value.
        q[i] = val;
    }
}

}  // namespace crl
