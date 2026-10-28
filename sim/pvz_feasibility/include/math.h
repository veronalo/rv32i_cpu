/* Declarations only, solely to compile the unmodified upstream math module.
 * Does NOT implement floating point or trigonometry, or claim they are ported.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
float sqrtf(float);
float sinf(float);
float cosf(float);
#ifdef __cplusplus
}
#endif
