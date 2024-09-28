#include <math.h>
#include <omp.h>

#include "global.h"
#include "img_data.h"
#include "cppdefs.h"
#include "mipp.h"

/*
 *
 * Martin Hart
 *
 * Soufiane El Mouahid
 *
 */

static void rotate(void);

#ifdef ENABLE_VECTO
#include <iostream>
static bool is_printed = false;
static void print_simd_info() {
  if (!is_printed) {
    std::cout << "SIMD infos:" << std::endl;
    std::cout << " - Instr. type:       " << mipp::InstructionType << std::endl;
    std::cout << " - Instr. full type:  " << mipp::InstructionFullType << std::endl;
    std::cout << " - Instr. version:    " << mipp::InstructionVersion << std::endl;
    std::cout << " - Instr. size:       " << mipp::RegisterSizeBit << " bits"
              << std::endl;
    std::cout << " - Instr. lanes:      " << mipp::Lanes << std::endl;
    std::cout << " - 64-bit support:    " << (mipp::Support64Bit ? "yes" : "no")
              << std::endl;
    std::cout << " - Byte/word support: " << (mipp::SupportByteWord ? "yes" : "no")
              << std::endl;
    auto ext = mipp::InstructionExtensions();
    if (ext.size() > 0) {
      std::cout << " - Instr. extensions: {";
      for (auto i = 0; i < (int)ext.size(); i++)
        std::cout << ext[i] << (i < ((int)ext.size() - 1) ? ", " : "");
      std::cout << "}" << std::endl;
    }
    std::cout << std::endl;
    is_printed = true;
  }
}
#endif

// Global variables
static float base_angle = 0.f;
static int color_a_r = 255, color_a_g = 255, color_a_b = 0, color_a_a = 255;
static int color_b_r = 0, color_b_g = 0, color_b_b = 255, color_b_a = 255;

// ----------------------------------------------------------------------------
// -------------------------------------------------------- INITIAL SEQ VERSION
// ----------------------------------------------------------------------------

// The image is a two-dimension array of size of DIM x DIM. Each pixel is of
// type 'unsigned' and store the color information following a RGBA layout (4
// bytes). Pixel at line 'l' and column 'c' in the current image can be accessed
// using cur_img (l, c).

// The kernel returns 0, or the iteration step at which computation has
// completed (e.g. stabilized).

// Computation of one pixel
//
//

/*
 * 1.2 Execution time according to optimization options on a Cortex A-57 core:
 *
 * -00 -> 21445.911
 * -01 -> 14777.032
 * -02 -> 13570.249 
 * -03 -> 13543.186
 *
 * With -ffast-math enabled:
 *
 * -00 -> 20612.951 
 * -01 -> 11972.732
 * -02 -> 11982.278
 * -03 -> 11917.901
 *
 * With -ffast-math enabled we can observe a performance gain on all optimization levels
 */
 
static unsigned compute_color(int i, int j) {
  float atan2f_in1 = (float)DIM / 2.f - (float)i;
  float atan2f_in2 = (float)j - (float)DIM / 2.f;
  float angle = atan2f(atan2f_in1, atan2f_in2) + M_PI + base_angle;

  float ratio = fabsf((fmodf(angle, M_PI / 4.f) - (M_PI / 8.f)) / (M_PI / 8.f));

  int r = color_a_r * ratio + color_b_r * (1.f - ratio);
  int g = color_a_g * ratio + color_b_g * (1.f - ratio);
  int b = color_a_b * ratio + color_b_b * (1.f - ratio);
  int a = color_a_a * ratio + color_b_a * (1.f - ratio);

  return ezv_rgba(r, g, b, a);
}

static void rotate(void) {
  base_angle = fmodf(base_angle + (1.f / 180.f) * M_PI, M_PI);
}

///////////////////////////// Simple sequential version (seq)
// Suggested cmdline(s):
// ./run --size 1024 --kernel spin --variant seq
// or
// ./run -s 1024 -k spin -v seq
//
EXTERN unsigned spin_compute_seq(unsigned nb_iter) {
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j++)
        cur_img(i, j) = compute_color(i, j);

    rotate();  // Slightly increase the base angle
  }

  return 0;
}

// ----------------------------------------------------------------------------
// --------------------------------------------------------- APPROX SEQ VERSION
// ----------------------------------------------------------------------------

/*
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 5453.690
 *
 * We can observe a 50% performance improvement compared to the previous version.
 * We explain it by the use of arithmetic operations that are less costly to the CPU.
 */
 
static float atanf_approx(float x) {
  return x * M_PI / 4.f + 0.273f * x * (1.f - fabsf(x));
}

static float atan2f_approx(float y, float x) {
  float ay = fabsf(y);
  float ax = fabsf(x);
  int invert = ay > ax;
  float z = invert ? ax / ay : ay / ax;
  float th = atanf_approx(z); // [0,pi/4]
  if (invert) th = M_PI_2 - th; // [0,pi/2]
  if (x < 0) th = M_PI - th; // [0,pi]
  if (y < 0) th = -th;
  return th;
}

static float fmodf_approx(float x, float y) {
  return x - trunc(x / y) * y;
}

static unsigned compute_color_approx(int i, int j) {
  float atan2f_in1 = (float)DIM / 2.f - (float)i;
  float atan2f_in2 = (float)j - (float)DIM / 2.f;
  float angle = atan2f_approx(atan2f_in1, atan2f_in2) + M_PI + base_angle;

  float ratio = fabsf((fmodf_approx(angle, M_PI / 4.f) - (M_PI / 8.f)) / (M_PI / 8.f));

  int r = color_a_r * ratio + color_b_r * (1.f - ratio);
  int g = color_a_g * ratio + color_b_g * (1.f - ratio);
  int b = color_a_b * ratio + color_b_b * (1.f - ratio);
  int a = color_a_a * ratio + color_b_a * (1.f - ratio);

  return ezv_rgba(r, g, b, a);
}

EXTERN unsigned spin_compute_approx(unsigned nb_iter) {
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j++)
        cur_img(i, j) = compute_color_approx(i, j);

    rotate();  // Slightly increase the base angle
  }

  return 0;
}

#ifdef ENABLE_VECTO
// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 0
// ----------------------------------------------------------------------------

/*
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 6321.565 
 *
 * We can observe a performance loss compared to the previous version.
 * We explain it by the introduction of the for loop, the size of it is known
 * at compile time but all the math operations inside of it are not
 * parallelized by SIMD instructions.
 */

// Computation of one pixel
static mipp::Reg<int> compute_color_simd_v0(mipp::Reg<int> r_i,
                                            mipp::Reg<int> r_j)
{
  int res[mipp::N<int>()];

  for (int it = 0; it < mipp::N<int>(); it++) {
    float atan2f_in1 = (float)DIM / 2.f - (float)r_i[0];
    float atan2f_in2 = (float)r_j[it] - (float)DIM / 2.f;
    float angle = atan2f_approx(atan2f_in1, atan2f_in2) + M_PI + base_angle;

    float ratio = fabsf((fmodf_approx(angle, M_PI / 4.f) - (M_PI / 8.f)) / (M_PI / 8.f));
    int r = color_a_r * ratio + color_b_r * (1.f - ratio);
    int g = color_a_g * ratio + color_b_g * (1.f - ratio);
    int b = color_a_b * ratio + color_b_b * (1.f - ratio);
    int a = color_a_a * ratio + color_b_a * (1.f - ratio);
    res[it] = ezv_rgba(r, g, b, a);
  }
  
  return mipp::Reg<int>(res);
}

EXTERN unsigned spin_compute_simd_v0(unsigned nb_iter) {
  print_simd_info();
  int tab_j[mipp::N<int>()];

  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        mipp::Reg<int> r_result =
            compute_color_simd_v0(mipp::Reg<int>(i), mipp::Reg<int>(tab_j));

        r_result.store(img_out_ptr);
      }

    rotate();  // Slightly increase the base angle
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 1
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 4477.413
 *
 * We observe a performance gain compared to the previous version.
 * We explain it by the parallelization (using SIMD instructions) of a good
 * portion of the math computation.
 */
 
static inline mipp::Reg<float> fmodf_approx_simd(mipp::Reg<float> r_x,
                                                 mipp::Reg<float> r_y) {
  return mipp::fnmadd(mipp::trunc(r_x / r_y), r_y, r_x);
}

// Computation of one pixel
static inline mipp::Reg<int> compute_color_simd_v1(mipp::Reg<int> r_i,
                                                   mipp::Reg<int> r_j)
{
  int res[mipp::N<int>()];
  float angles[mipp::N<int>()];

  for (int it = 0; it < mipp::N<int>(); it++) {
      float atan2f_in1 = (float)DIM / 2.f - (float)r_i[it];
      float atan2f_in2 = (float)r_j[it] - (float)DIM / 2.f;
      angles[it] = atan2f_approx(atan2f_in1, atan2f_in2) + M_PI + base_angle;
  }

  mipp::Reg<float> r_ratio = mipp::abs((fmodf_approx_simd(mipp::Reg<float>(angles),
                  mipp::Reg<float>(M_PI / 4.f)) - M_PI / 8.f) / (M_PI / 8.f));

  for (int it = 0; it < mipp::N<int>(); it++) {
    int r = color_a_r * r_ratio[it] + color_b_r * (1.f - r_ratio[it]);
    int g = color_a_g * r_ratio[it] + color_b_g * (1.f - r_ratio[it]);
    int b = color_a_b * r_ratio[it] + color_b_b * (1.f - r_ratio[it]);
    int a = color_a_a * r_ratio[it] + color_b_a * (1.f - r_ratio[it]);
    res[it] = ezv_rgba(r, g, b, a);
  }
  
  return mipp::Reg<int>(res);
}


EXTERN unsigned spin_compute_simd_v1(unsigned nb_iter) {
  int tab_j[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        mipp::Reg<int> r_result =
            compute_color_simd_v1(mipp::Reg<int>(i), mipp::Reg<int>(tab_j));

        r_result.store(img_out_ptr);
      }

    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 2
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 4306.639
 *
 * We observe an improvement compared to the previous version.
 * We explain it by the added parallelization of the rgba calculation.
 */

static inline mipp::Reg<int> rgba_simd(mipp::Reg<int> r_r, mipp::Reg<int> r_g,
                                       mipp::Reg<int> r_b, mipp::Reg<int> r_a) {
  return r_r | (r_g << 8) | (r_b << 16) | (r_a << 24);
}

// Computation of one pixel
static inline mipp::Reg<int> compute_color_simd_v2(mipp::Reg<int> r_i,
                                                   mipp::Reg<int> r_j)
{
  float angles[mipp::N<int>()];

  for (int it = 0; it < mipp::N<int>(); it++) {
      float atan2f_in1 = (float)DIM / 2.f - (float)r_i[it];
      float atan2f_in2 = (float)r_j[it] - (float)DIM / 2.f;
      angles[it] = atan2f_approx(atan2f_in1, atan2f_in2) + M_PI + base_angle;
  }

  mipp::Reg<float> r_ratio = mipp::abs((fmodf_approx_simd(mipp::Reg<float>(angles),
                  mipp::Reg<float>(M_PI / 4.f)) - M_PI / 8.f) / (M_PI / 8.f));
                  
  mipp::Reg<int> r_r = mipp::cvt<float, int>(r_ratio * color_a_r + (- r_ratio + 1.f) * color_b_r);
  mipp::Reg<int> r_g = mipp::cvt<float, int>(r_ratio * color_a_g + (- r_ratio + 1.f) * color_b_g);
  mipp::Reg<int> r_b = mipp::cvt<float, int>(r_ratio * color_a_b + (- r_ratio + 1.f) * color_b_b);
  mipp::Reg<int> r_a = mipp::cvt<float, int>(r_ratio * color_a_a + (- r_ratio + 1.f) * color_b_a);
  
  return rgba_simd(r_r, r_g, r_b, r_a);
}

EXTERN unsigned spin_compute_simd_v2(unsigned nb_iter) {
  int tab_j[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        mipp::Reg<int> r_result =
            compute_color_simd_v2(mipp::Reg<int>(i), mipp::Reg<int>(tab_j));

        r_result.store(img_out_ptr);
      }

    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 3
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2946.744
 *
 * We observe a massive improvement compared to the previous version.
 * The entire calculation is now parallelized.
 */

static inline mipp::Reg<float> atanf_approx_simd(mipp::Reg<float> r_z) {
  return r_z * ((- mipp::abs(r_z) + 1.f) * 0.273f + M_PI / 4.f);
}

static inline mipp::Reg<float> atan2f_approx_simd(mipp::Reg<float> r_y,
                                                  mipp::Reg<float> r_x) {
  mipp::Reg<float> r_ay = mipp::abs(r_y);
  mipp::Reg<float> r_ax = mipp::abs(r_x);
  mipp::Msk<mipp::N<float>()> r_invert = r_ay > r_ax;
  mipp::Reg<float> r_z = mipp::blend(r_ax / r_ay, r_ay / r_ax, r_invert);
  mipp::Reg<float> r_th = atanf_approx_simd(r_z);

  mipp::Msk<mipp::N<float>()> r_m1 = r_x < 0.f;
  mipp::Msk<mipp::N<float>()> r_m2 = r_y < 0.f;
  r_th = mipp::blend(-r_th + M_PI_2, r_th, r_invert);
  r_th = mipp::blend(-r_th + M_PI, r_th, r_m1);
  r_th = mipp::blend(-r_th, r_th, r_m2);
  
  return r_th;
}

// Computation of one pixel
static inline mipp::Reg<int> compute_color_simd_v3(mipp::Reg<int> r_i,
                                                   mipp::Reg<int> r_j)
{
  mipp::Reg<float> r_atan2f_in1 = - mipp::cvt<int, float>(r_i) + (float)DIM / 2.f;
  mipp::Reg<float> r_atan2f_in2 = mipp::cvt<int, float>(r_j) - (float)DIM / 2.f;
  mipp::Reg<float> r_angles = atan2f_approx_simd(r_atan2f_in1, r_atan2f_in2) + (M_PI + base_angle);
  
  mipp::Reg<float> r_ratio = mipp::abs((fmodf_approx_simd(r_angles, M_PI / 4.f) -
			  M_PI / 8.f) / (M_PI / 8.f));
  
  mipp::Reg<int> r_r = mipp::cvt<float, int>(r_ratio * color_a_r + (- r_ratio + 1.f) * color_b_r);
  mipp::Reg<int> r_g = mipp::cvt<float, int>(r_ratio * color_a_g + (- r_ratio + 1.f) * color_b_g);
  mipp::Reg<int> r_b = mipp::cvt<float, int>(r_ratio * color_a_b + (- r_ratio + 1.f) * color_b_b);
  mipp::Reg<int> r_a = mipp::cvt<float, int>(r_ratio * color_a_a + (- r_ratio + 1.f) * color_b_a);
  
  return rgba_simd(r_r, r_g, r_b, r_a);
}

EXTERN unsigned spin_compute_simd_v3(unsigned nb_iter) {
  int tab_j[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        mipp::Reg<int> r_result =
            compute_color_simd_v3(mipp::Reg<int>(i), mipp::Reg<int>(tab_j));

        r_result.store(img_out_ptr);
      }

    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 4
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2928.286
 *
 * We observe a slight improvement which is probably a measurement error
 * as the functions were already inlined in the previous version.
 */
 
// Computation of one pixel
static inline mipp::Reg<int> compute_color_simd_v4(mipp::Reg<int> r_i,
                                                   mipp::Reg<int> r_j)
{  
  mipp::Reg<float> r_atan2f_in1 = - mipp::cvt<int, float>(r_i) + (float)DIM / 2.f;
  mipp::Reg<float> r_atan2f_in2 = mipp::cvt<int, float>(r_j) - (float)DIM / 2.f;

  mipp::Reg<float> r_ay = mipp::abs(r_atan2f_in1);
  mipp::Reg<float> r_ax = mipp::abs(r_atan2f_in2);
  mipp::Msk<mipp::N<float>()> r_invert = r_ay > r_ax;
  mipp::Reg<float> r_z = mipp::blend(r_ax / r_ay, r_ay / r_ax, r_invert);
  mipp::Reg<float> r_th = r_z * ((- mipp::abs(r_z) + 1.f) * 0.273f + M_PI / 4.f);

  mipp::Msk<mipp::N<float>()> r_m1 = r_atan2f_in2 < 0.f;
  mipp::Msk<mipp::N<float>()> r_m2 = r_atan2f_in1 < 0.f;
  r_th = mipp::blend(-r_th + M_PI_2, r_th, r_invert);
  r_th = mipp::blend(-r_th + M_PI, r_th, r_m1);
  r_th = mipp::blend(-r_th, r_th, r_m2);

  mipp::Reg<float> r_ratio = mipp::abs((fmodf_approx_simd(r_th + (M_PI + base_angle), M_PI / 4.f) -
			  M_PI / 8.f) / (M_PI / 8.f));

  mipp::Reg<int> r_r = mipp::cvt<float, int>(r_ratio * color_a_r + (- r_ratio + 1.f) * color_b_r);
  mipp::Reg<int> r_g = mipp::cvt<float, int>(r_ratio * color_a_g + (- r_ratio + 1.f) * color_b_g);
  mipp::Reg<int> r_b = mipp::cvt<float, int>(r_ratio * color_a_b + (- r_ratio + 1.f) * color_b_b);
  mipp::Reg<int> r_a = mipp::cvt<float, int>(r_ratio * color_a_a + (- r_ratio + 1.f) * color_b_a);
  
  return rgba_simd(r_r, r_g, r_b, r_a);
}

EXTERN unsigned spin_compute_simd_v4(unsigned nb_iter) {
  int tab_j[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++)
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        mipp::Reg<int> r_result =
            compute_color_simd_v4(mipp::Reg<int>(i), mipp::Reg<int>(tab_j));

        r_result.store(img_out_ptr);
      }

    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 5
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2888.721
 *
 * Not a massive improvement as the function was 
 * already inlined by the compiler.
 */

EXTERN unsigned spin_compute_simd_v5(unsigned nb_iter) {
  int tab_j[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++) {
      mipp::Reg<float> r_atan2f_in1 = mipp::fmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              - mipp::cvt<int, float>(mipp::Reg<int>(i)));
      mipp::Reg<float> r_ay = mipp::abs(r_atan2f_in1);
      mipp::Msk<mipp::N<float>()> r_m2 = r_atan2f_in1 < 0.f;
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j[jj] = j + jj;
        int* img_out_ptr = (int*)&cur_img(i, j);
        
        mipp::Reg<float> r_atan2f_in2 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j)));

        mipp::Reg<float> r_ax = mipp::abs(r_atan2f_in2);
        mipp::Msk<mipp::N<float>()> r_invert = r_ay > r_ax;
        mipp::Reg<float> r_z = mipp::blend(r_ax / r_ay, r_ay / r_ax, r_invert);
        mipp::Reg<float> r_th = r_z * mipp::fmadd((- mipp::abs(r_z) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1 = r_atan2f_in2 < 0.f;
        r_th = mipp::blend(-r_th + M_PI_2, r_th, r_invert);
        r_th = mipp::blend(-r_th + M_PI, r_th, r_m1);
        r_th = mipp::blend(-r_th, r_th, r_m2);

        mipp::Reg<float> r_ratio = mipp::abs((fmodf_approx_simd(r_th + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r = mipp::cvt<float, int>(mipp::fmadd(r_ratio, 
                          mipp::Reg<float>(color_a_r), (- r_ratio + 1.f) * color_b_r));
        mipp::Reg<int> r_g = mipp::cvt<float, int>(mipp::fmadd(r_ratio, 
                          mipp::Reg<float>(color_a_g), (- r_ratio + 1.f) * color_b_g));
        mipp::Reg<int> r_b = mipp::cvt<float, int>(mipp::fmadd(r_ratio, 
                          mipp::Reg<float>(color_a_b), (- r_ratio + 1.f) * color_b_b));
        mipp::Reg<int> r_a = mipp::cvt<float, int>(mipp::fmadd(r_ratio, 
                          mipp::Reg<float>(color_a_a), (- r_ratio + 1.f) * color_b_a));

        rgba_simd(r_r, r_g, r_b, r_a).store(img_out_ptr);
      }
    }
    
    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

// ----------------------------------------------------------------------------
// ------------------------------------------------------------- SIMD VERSION 6
// ----------------------------------------------------------------------------

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2807.818
 */

EXTERN unsigned spin_compute_simd_v6(unsigned nb_iter) {
  int tab_j_j0[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++) {
      mipp::Reg<float> r_atan2f_in1 = mipp::fmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              - mipp::cvt<int, float>(mipp::Reg<int>(i)));
      mipp::Reg<float> r_ay = mipp::abs(r_atan2f_in1);
      mipp::Msk<mipp::N<float>()> r_m2 = r_atan2f_in1 < 0.f;
      for (unsigned j = 0; j < DIM; j += mipp::N<float>()) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j0[jj] = j + jj;
        int* img_out_ptr_j0 = (int*)&cur_img(i, j);
        
        mipp::Reg<float> r_atan2f_in2_j0 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j0)));

        mipp::Reg<float> r_ax_j0 = mipp::abs(r_atan2f_in2_j0);
        mipp::Msk<mipp::N<float>()> r_invert_j0 = r_ay > r_ax_j0;
        mipp::Reg<float> r_z_j0 = mipp::blend(r_ax_j0 / r_ay, r_ay / r_ax_j0, r_invert_j0);
        mipp::Reg<float> r_th_j0 = r_z_j0 * mipp::fmadd((- mipp::abs(r_z_j0) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j0 = r_atan2f_in2_j0 < 0.f;
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI_2, r_th_j0, r_invert_j0);
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI, r_th_j0, r_m1_j0);
        r_th_j0 = mipp::blend(-r_th_j0, r_th_j0, r_m2);

        mipp::Reg<float> r_ratio_j0 = mipp::abs((fmodf_approx_simd(r_th_j0 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j0 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j0 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j0 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j0 + 1.f) * color_b_a));

        rgba_simd(r_r_j0, r_g_j0, r_b_j0, r_a_j0).store(img_out_ptr_j0);
      }
    }
    
    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2878.432
 *
 * Slight improvement explained by the use of multiple cores
 * when executing instructions for different j values that 
 * can be executed in parallel
 */

EXTERN unsigned spin_compute_simd_v6u2(unsigned nb_iter) {
  int tab_j_j0[mipp::N<int>()], tab_j_j1[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++) {
      mipp::Reg<float> r_atan2f_in1 = mipp::fmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              - mipp::cvt<int, float>(mipp::Reg<int>(i)));
      mipp::Reg<float> r_ay = mipp::abs(r_atan2f_in1);
      mipp::Msk<mipp::N<float>()> r_m2 = r_atan2f_in1 < 0.f;
      for (unsigned j = 0; j < DIM; j += mipp::N<float>() * 2) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j0[jj] = j + jj;
        int* img_out_ptr_j0 = (int*)&cur_img(i, j);
        
        mipp::Reg<float> r_atan2f_in2_j0 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j0)));

        mipp::Reg<float> r_ax_j0 = mipp::abs(r_atan2f_in2_j0);
        mipp::Msk<mipp::N<float>()> r_invert_j0 = r_ay > r_ax_j0;
        mipp::Reg<float> r_z_j0 = mipp::blend(r_ax_j0 / r_ay, r_ay / r_ax_j0, r_invert_j0);
        mipp::Reg<float> r_th_j0 = r_z_j0 * mipp::fmadd((- mipp::abs(r_z_j0) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j0 = r_atan2f_in2_j0 < 0.f;
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI_2, r_th_j0, r_invert_j0);
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI, r_th_j0, r_m1_j0);
        r_th_j0 = mipp::blend(-r_th_j0, r_th_j0, r_m2);

        mipp::Reg<float> r_ratio_j0 = mipp::abs((fmodf_approx_simd(r_th_j0 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j0 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j0 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j0 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j0 + 1.f) * color_b_a));

        rgba_simd(r_r_j0, r_g_j0, r_b_j0, r_a_j0).store(img_out_ptr_j0);
        
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j1[jj] = j + mipp::N<float>() + jj;
        int* img_out_ptr_j1 = (int*)&cur_img(i, j + mipp::N<float>());
        
        mipp::Reg<float> r_atan2f_in2_j1 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j1)));

        mipp::Reg<float> r_ax_j1 = mipp::abs(r_atan2f_in2_j1);
        mipp::Msk<mipp::N<float>()> r_invert_j1 = r_ay > r_ax_j1;
        mipp::Reg<float> r_z_j1 = mipp::blend(r_ax_j1 / r_ay, r_ay / r_ax_j1, r_invert_j1);
        mipp::Reg<float> r_th_j1 = r_z_j1 * mipp::fmadd((- mipp::abs(r_z_j1) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j1 = r_atan2f_in2_j1 < 0.f;
        r_th_j1 = mipp::blend(-r_th_j1 + M_PI_2, r_th_j1, r_invert_j1);
        r_th_j1 = mipp::blend(-r_th_j1 + M_PI, r_th_j1, r_m1_j1);
        r_th_j1 = mipp::blend(-r_th_j1, r_th_j1, r_m2);

        mipp::Reg<float> r_ratio_j1 = mipp::abs((fmodf_approx_simd(r_th_j1 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j1 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j1 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j1 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j1 + 1.f) * color_b_a));

        rgba_simd(r_r_j1, r_g_j1, r_b_j1, r_a_j1).store(img_out_ptr_j1);
      }
    }
    
    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

/* 
 * Execution time with `-03 -ffast-math` optimization options
 * on a Cortex A-57 core: 2814.185
 *
 * Slight improvement explained by the presence of 4 cores
 * so no improvement expected between this version and the
 * previous version when run on the Denver
 */

EXTERN unsigned spin_compute_simd_v6u4(unsigned nb_iter) {
  int tab_j_j0[mipp::N<int>()], tab_j_j1[mipp::N<int>()], tab_j_j2[mipp::N<int>()], tab_j_j3[mipp::N<int>()];
  
  for (unsigned it = 1; it <= nb_iter; it++) {
    for (unsigned i = 0; i < DIM; i++) {
      mipp::Reg<float> r_atan2f_in1 = mipp::fmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              - mipp::cvt<int, float>(mipp::Reg<int>(i)));
      mipp::Reg<float> r_ay = mipp::abs(r_atan2f_in1);
      mipp::Msk<mipp::N<float>()> r_m2 = r_atan2f_in1 < 0.f;
      for (unsigned j = 0; j < DIM; j += mipp::N<float>() * 4) {
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j0[jj] = j + jj;
        int* img_out_ptr_j0 = (int*)&cur_img(i, j);
        
        mipp::Reg<float> r_atan2f_in2_j0 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j0)));

        mipp::Reg<float> r_ax_j0 = mipp::abs(r_atan2f_in2_j0);
        mipp::Msk<mipp::N<float>()> r_invert_j0 = r_ay > r_ax_j0;
        mipp::Reg<float> r_z_j0 = mipp::blend(r_ax_j0 / r_ay, r_ay / r_ax_j0, r_invert_j0);
        mipp::Reg<float> r_th_j0 = r_z_j0 * mipp::fmadd((- mipp::abs(r_z_j0) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j0 = r_atan2f_in2_j0 < 0.f;
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI_2, r_th_j0, r_invert_j0);
        r_th_j0 = mipp::blend(-r_th_j0 + M_PI, r_th_j0, r_m1_j0);
        r_th_j0 = mipp::blend(-r_th_j0, r_th_j0, r_m2);

        mipp::Reg<float> r_ratio_j0 = mipp::abs((fmodf_approx_simd(r_th_j0 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j0 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j0 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j0 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j0 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j0, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j0 + 1.f) * color_b_a));

        rgba_simd(r_r_j0, r_g_j0, r_b_j0, r_a_j0).store(img_out_ptr_j0);
        
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j1[jj] = j + mipp::N<float>() + jj;
        int* img_out_ptr_j1 = (int*)&cur_img(i, j + mipp::N<float>());
        
        mipp::Reg<float> r_atan2f_in2_j1 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j1)));

        mipp::Reg<float> r_ax_j1 = mipp::abs(r_atan2f_in2_j1);
        mipp::Msk<mipp::N<float>()> r_invert_j1 = r_ay > r_ax_j1;
        mipp::Reg<float> r_z_j1 = mipp::blend(r_ax_j1 / r_ay, r_ay / r_ax_j1, r_invert_j1);
        mipp::Reg<float> r_th_j1 = r_z_j1 * mipp::fmadd((- mipp::abs(r_z_j1) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j1 = r_atan2f_in2_j1 < 0.f;
        r_th_j1 = mipp::blend(-r_th_j1 + M_PI_2, r_th_j1, r_invert_j1);
        r_th_j1 = mipp::blend(-r_th_j1 + M_PI, r_th_j1, r_m1_j1);
        r_th_j1 = mipp::blend(-r_th_j1, r_th_j1, r_m2);

        mipp::Reg<float> r_ratio_j1 = mipp::abs((fmodf_approx_simd(r_th_j1 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j1 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j1 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j1 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j1 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j1, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j1 + 1.f) * color_b_a));

        rgba_simd(r_r_j1, r_g_j1, r_b_j1, r_a_j1).store(img_out_ptr_j1);
        
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j2[jj] = j + mipp::N<float>() * 2 + jj;
        int* img_out_ptr_j2 = (int*)&cur_img(i, j + mipp::N<float>() * 2);
        
        mipp::Reg<float> r_atan2f_in2_j2 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j2)));

        mipp::Reg<float> r_ax_j2 = mipp::abs(r_atan2f_in2_j2);
        mipp::Msk<mipp::N<float>()> r_invert_j2 = r_ay > r_ax_j2;
        mipp::Reg<float> r_z_j2 = mipp::blend(r_ax_j2 / r_ay, r_ay / r_ax_j2, r_invert_j2);
        mipp::Reg<float> r_th_j2 = r_z_j2 * mipp::fmadd((- mipp::abs(r_z_j2) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j2 = r_atan2f_in2_j2 < 0.f;
        r_th_j2 = mipp::blend(-r_th_j2 + M_PI_2, r_th_j2, r_invert_j2);
        r_th_j2 = mipp::blend(-r_th_j2 + M_PI, r_th_j2, r_m1_j2);
        r_th_j2 = mipp::blend(-r_th_j2, r_th_j2, r_m2);

        mipp::Reg<float> r_ratio_j2 = mipp::abs((fmodf_approx_simd(r_th_j2 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j2 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j2, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j2 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j2 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j2, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j2 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j2 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j2, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j2 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j2 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j2, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j2 + 1.f) * color_b_a));

        rgba_simd(r_r_j2, r_g_j2, r_b_j2, r_a_j2).store(img_out_ptr_j2);
        
        for (unsigned jj = 0; jj < mipp::N<float>(); jj++) tab_j_j3[jj] = j + mipp::N<float>() * 3 + jj;
        int* img_out_ptr_j3 = (int*)&cur_img(i, j + mipp::N<float>() * 3);
        
        mipp::Reg<float> r_atan2f_in2_j3 = mipp::fnmadd(mipp::Reg<float>(DIM), mipp::Reg<float>(1.f / 2.f), 
                              mipp::cvt<int,float>(mipp::Reg<int>(tab_j_j3)));

        mipp::Reg<float> r_ax_j3 = mipp::abs(r_atan2f_in2_j3);
        mipp::Msk<mipp::N<float>()> r_invert_j3 = r_ay > r_ax_j3;
        mipp::Reg<float> r_z_j3 = mipp::blend(r_ax_j3 / r_ay, r_ay / r_ax_j3, r_invert_j3);
        mipp::Reg<float> r_th_j3 = r_z_j3 * mipp::fmadd((- mipp::abs(r_z_j3) + 1.f), mipp::Reg<float>(0.273f), 
                              mipp::Reg<float>(M_PI / 4.f));
        mipp::Msk<mipp::N<float>()> r_m1_j3 = r_atan2f_in2_j3 < 0.f;
        r_th_j3 = mipp::blend(-r_th_j3 + M_PI_2, r_th_j3, r_invert_j3);
        r_th_j3 = mipp::blend(-r_th_j3 + M_PI, r_th_j3, r_m1_j3);
        r_th_j3 = mipp::blend(-r_th_j3, r_th_j3, r_m2);

        mipp::Reg<float> r_ratio_j3 = mipp::abs((fmodf_approx_simd(r_th_j3 + (M_PI + base_angle), M_PI / 4.f) -
			        M_PI / 8.f) / (M_PI / 8.f));

        mipp::Reg<int> r_r_j3 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j3, 
                          mipp::Reg<float>(color_a_r), (- r_ratio_j3 + 1.f) * color_b_r));
        mipp::Reg<int> r_g_j3 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j3, 
                          mipp::Reg<float>(color_a_g), (- r_ratio_j3 + 1.f) * color_b_g));
        mipp::Reg<int> r_b_j3 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j3, 
                          mipp::Reg<float>(color_a_b), (- r_ratio_j3 + 1.f) * color_b_b));
        mipp::Reg<int> r_a_j3 = mipp::cvt<float, int>(mipp::fmadd(r_ratio_j3, 
                          mipp::Reg<float>(color_a_a), (- r_ratio_j3 + 1.f) * color_b_a));

        rgba_simd(r_r_j3, r_g_j3, r_b_j3, r_a_j3).store(img_out_ptr_j3);
      }
    }
    
    rotate();  // Slightly increase the base angle
  }
  
  return 0;
}

#endif /* ENABLE_VECTO */
