// JNI bridge for cultris.NativeAi -> libcultrisai (c2 AI dylib).
//
// Symbol prefix follows the JNI mangling for the Java package `cultris`:
//   Java_cultris_NativeAi_<method>
//
// The Java layer packs the board into a `long[28]` where rowBits[0] is the
// bottom row and bit 0 is the leftmost column. We forward the buffer straight
// into C2StepInput without endian / order remapping so the native library must
// share the same convention (documented in c2_ai_api.h).

#include <jni.h>
#include "c2_ai_api.h"

#include <cstring>
#include <cstdint>

extern "C" {

JNIEXPORT jstring JNICALL
Java_cultris_NativeAi_nativeGetAiName(JNIEnv* env, jclass /*klass*/) {
    const char* name = c2ai_name();
    return name == nullptr ? nullptr : env->NewStringUTF(name);
}

JNIEXPORT jlong JNICALL
Java_cultris_NativeAi_nativeCreate(JNIEnv* /*env*/, jclass /*klass*/) {
    return static_cast<jlong>(c2ai_create());
}

JNIEXPORT void JNICALL
Java_cultris_NativeAi_nativeDestroy(JNIEnv* /*env*/, jclass /*klass*/, jlong handle) {
    if (handle == 0) {
        return;
    }
    c2ai_destroy(static_cast<int64_t>(handle));
}

JNIEXPORT jint JNICALL
Java_cultris_NativeAi_nativeStep(JNIEnv* env, jclass /*klass*/, jlong handle,
                                 jlongArray rowBits,
                                 jint curKind, jint curX, jint curY, jint curR,
                                 jint nextKind,
                                 jfloat attackCurveScale, jfloat pressureBlend,
                                 jint incomingGarbage, jint comboCount,
                                 jint planningBpm, jint comboRemainingMs,
                                 jboolean softDrop, jint safe,
                                 jlong pieceSerial, jlong boardGen) {
    if (handle == 0 || rowBits == nullptr) {
        return -1;
    }
    const jsize len = env->GetArrayLength(rowBits);
    if (len != 28) {
        return -1;
    }
    jlong* rows = env->GetLongArrayElements(rowBits, nullptr);
    if (rows == nullptr) {
        return -1;
    }

    C2StepInput in{};
    in.api_version = C2_AI_API_VERSION; // 4
    std::memcpy(in.row_bits, rows, sizeof(uint64_t) * 28);
    env->ReleaseLongArrayElements(rowBits, rows, JNI_ABORT);

    in.cur_kind = static_cast<int32_t>(curKind);
    in.cur_x    = static_cast<int32_t>(curX);
    in.cur_y    = static_cast<int32_t>(curY);
    in.cur_r    = static_cast<int32_t>(curR);
    in.next_kind = static_cast<int32_t>(nextKind);
    in.attack_curve_scale = static_cast<float>(attackCurveScale);
    in.pressure_blend     = static_cast<float>(pressureBlend);
    in.incoming_garbage   = static_cast<int32_t>(incomingGarbage);
    in.combo_count        = static_cast<int32_t>(comboCount);
    in.piece_serial       = static_cast<int64_t>(pieceSerial);
    in.board_gen          = static_cast<int64_t>(boardGen);
    // Keep the Java API explicit while preserving the C ABI layout.
    in.reserved[C2_INPUT_SOFT_DROP] = softDrop == JNI_TRUE ? 1 : 0;
    in.reserved[C2_INPUT_SAFE] = static_cast<int32_t>(safe);
    in.reserved[C2_INPUT_PLANNING_BPM] = static_cast<int32_t>(planningBpm);
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = static_cast<int32_t>(comboRemainingMs);

    C2StepOutput out{};
    c2ai_step(static_cast<int64_t>(handle), &in, &out);

    // -1 covers every non-OK status. The Java layer treats any negative
    // return as "skip this frame without consuming actionTimer or the
    // fail streak", which matches both SEARCHING (async worker still
    // computing) and the truly-broken NO_MOVE / INVALID_INPUT cases.
    if (out.status != C2_STATUS_OK) {
        return -1;
    }
    return static_cast<jint>(out.next_op);
}

JNIEXPORT void JNICALL
Java_cultris_NativeAi_nativeSetMode(JNIEnv* /*env*/, jclass /*klass*/,
                                    jlong handle, jint aiMode) {
    if (handle == 0) {
        return;
    }
    c2ai_set_mode(static_cast<int64_t>(handle), static_cast<int32_t>(aiMode));
}

JNIEXPORT void JNICALL
Java_cultris_NativeAi_nativeResetPlan(JNIEnv* /*env*/, jclass /*klass*/, jlong handle) {
    if (handle == 0) {
        return;
    }
    c2ai_reset_plan(static_cast<int64_t>(handle));
}

JNIEXPORT void JNICALL
Java_cultris_NativeAi_nativeSetIterateRange(JNIEnv* /*env*/, jclass /*klass*/,
                                            jlong handle, jint low, jint high) {
    if (handle == 0) {
        return;
    }
    c2ai_set_iterate_range(static_cast<int64_t>(handle),
                           static_cast<int32_t>(low),
                           static_cast<int32_t>(high));
}

JNIEXPORT void JNICALL
Java_cultris_NativeAi_nativeSetNextMode(JNIEnv* /*env*/, jclass /*klass*/,
                                        jlong handle,
                                        jboolean virtualFlag, jboolean preview) {
    if (handle == 0) {
        return;
    }
    c2ai_set_next_mode(static_cast<int64_t>(handle),
                       virtualFlag == JNI_TRUE ? 1 : 0,
                       preview == JNI_TRUE ? 1 : 0);
}

} // extern "C"
