#include "plane.h"
#include "constants.h"

#include <math.h>

// plane stuff ------------------------------------------------------------------------------------

plane::plane(){}

plane::plane(vec3 A, vec3 B, vec3 C){
    initialize(A, B, C);
}

void plane::initialize(vec3 A, vec3 B, vec3 C){
    point = A;
    vec3 leg1 = B - A;
    vec3 leg2 = C - A;
    normal = CrossProduct(leg1, leg2);
    normal.normalize();
    return;
}


// returns true if ray intersects plane at a single point, and puts that point in contactpoint
// contactpoint - stores output here
// rayOrigin - origin point of ray
// rayDirection - direction of ray
// planeNormal - normal of the plane
// plane Coord - an arbitrary point on the plane
bool linePlaneIntersection(vec3 &contactpoint, vec3 rayOrigin, vec3 rayDirection, vec3 planeNormal, vec3 planeCoord) {
        // normalize stuff
        rayDirection.normalize();
        planeNormal.normalize();
        vec3 Diff = planeCoord - rayOrigin;
        double d = DotProduct(planeNormal, Diff);
        double e = DotProduct(planeNormal, rayDirection);
        if (fabs(e) > EPSILONZERO){
            contactpoint = rayOrigin + (rayDirection * (d / e));
            return true;
        }
        /*
        else {
            printf("dot product of %f, %f, %f, and %f, %f, %f is 0\n", planeNormal.x, planeNormal.y, planeNormal.z, rayDirection.x, rayDirection.y, rayDirection.z);
        }
        */
        return false;
}
