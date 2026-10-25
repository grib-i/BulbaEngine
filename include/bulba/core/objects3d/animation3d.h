#ifndef BULBA_CORE_OBJECTS3D_ANIMATION3D_H
#define BULBA_CORE_OBJECTS3D_ANIMATION3D_H

#include "bulba/core/objects3d/objects3d.h"

#include <stdbool.h>
#include <stddef.h>

size_t BLB_Model_GetBoneCount(const BLB_Object3D *object);
const char *BLB_Model_GetBoneName(const BLB_Object3D *object, size_t index);
int BLB_Model_SetBonePosition(BLB_Object3D *object, const char *name, BLB_Vec3 position);
int BLB_Model_SetBoneRotation(BLB_Object3D *object, const char *name, BLB_Vec3 rotation_degrees);
int BLB_Model_SetBoneScale(BLB_Object3D *object, const char *name, BLB_Vec3 scale);
int BLB_Model_SetBoneTransform(BLB_Object3D *object, const char *name, BLB_Vec3 position, BLB_Vec3 rotation_degrees, BLB_Vec3 scale);
int BLB_Model_ResetBone(BLB_Object3D *object, const char *name);

size_t BLB_Model_GetAnimationCount(const BLB_Object3D *object);
const char *BLB_Model_GetAnimationName(const BLB_Object3D *object, size_t index);
int BLB_Model_PlayAnimation(BLB_Object3D *object, const char *name, bool loop);
void BLB_Model_StopAnimation(BLB_Object3D *object);
void BLB_Model_SetAnimationSpeed(BLB_Object3D *object, float speed);
int BLB_Model_SetAnimationTime(BLB_Object3D *object, float seconds);
float BLB_Model_GetAnimationTime(const BLB_Object3D *object);
/* Active clip duration in seconds; zero when no clip is selected. */
float BLB_Model_GetAnimationDuration(const BLB_Object3D *object);
/* BLB_DrawScene advances scene objects automatically. Use this directly only for
 * offline stepping or objects that are not advanced by a scene draw. */
void BLB_Model_UpdateAnimation(BLB_Object3D *object, float delta_time);

#endif
