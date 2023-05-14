#include "plane.h"

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
    // we'll want this for hardcoding the planes in the shader language
    //printf("plane initialized with point %f, %f, %f and normal %f, %f, %f\n", point.x, point.y, point.z, normal.x, normal.y, normal.z); 
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
        //printf("diff is %f, %f, %f\n", Diff.x, Diff.y, Diff.z);
        double d = DotProduct(planeNormal, Diff);
        //printf("d is %f\n", d);
        double e = DotProduct(planeNormal, rayDirection);
       // printf("e is %f\n", e);
        if (fabs(e) > 1e-7){ // 1e-8 seems to be about where rounding errors start to creep in 
            contactpoint = rayOrigin + (rayDirection * (d / e));
            return true;
        }
        return false;
}
