#pragma once

#include "types.hpp"

#define NO_NUMERIC_CMATH

#ifdef NO_NUMERIC_CMATH

namespace numeric
{
    inline constexpr f64 fma(f64 a, f64 b, f64 c)
    {
        return a * b + c;
    }


    inline constexpr f32 fmaf(f32 a, f32 b, f32 c)
    {
        return a * b + c;
    }
}

#else

// WARNING: no constexpr fma
#include <cmath>

#endif



namespace numeric
{
    template <typename T>
    inline constexpr bool is_unsigned()
    {
        return (T)((T)0 - (T)1) > (T)0;
    }


    template <typename T>
    inline constexpr T clamp(T value, T min, T max)
    {
        const T t = value < min ? min : value;
        return t > max ? max : t;
    }


    template <typename T, typename U>
    inline constexpr T clamp(T value, U min, U max)
    {
        const T t = value < min ? (T)min : value;
        return t > max ? (T)max : t;
    }


    template <typename R, typename T>
    inline constexpr R sign(T value)
    {
        static_assert(!is_unsigned<T>());

        constexpr R P = (R)1;
        constexpr R N = (R)-1;
        constexpr R Z = (R)0;

        return value ? ((f32)value < 0.0f ? N : P) : Z;
    }


    template <typename T>
    inline constexpr T round_to_unsigned(f32 value)
    {
        static_assert(is_unsigned<T>());

        return (T)(value + 0.5f);
    }


    template <typename T>
    inline constexpr T round_to_unsigned(f64 value)
    {
        static_assert(is_unsigned<T>());

        return (T)(value + 0.5);
    }


    template <typename T>
    inline constexpr T cxpr_round_to_signed(f32 value)
    {
        static_assert(!is_unsigned<T>());

        return (T)(value + sign<f32, f32>(value) * 0.5f);
    }


    template <typename T>
    inline constexpr T cxpr_round_to_signed(f64 value)
    {
        static_assert(!is_unsigned<T>());

        return (T)(value + sign<f64, f64>(value) * 0.5);
    }


    template <typename T>
    inline constexpr T round_to_signed(f32 value)
    {
        static_assert(!is_unsigned<T>());

        return (T)fmaf(sign<f32, f32>(value), 0.5f, value);
    }


    template <typename T>
    inline constexpr T round_to_signed(f64 value)
    {
        static_assert(!is_unsigned<T>());

        return (T)fma(sign<f64, f64>(value), 0.5, value);
    }


    inline constexpr f32 pow(f32 base, u32 exp)
    {
        f32 val = 1.0f;
        for (u32 i = 0; i < exp; i++)
        {
            val *= base;
        }

        return val;
    }


    template <size_t N>
    inline constexpr f32 round(f32 value)
    {
        constexpr auto f = pow(10.0f, N);
        constexpr auto i_f = 1.0f / f;

        return round_to_signed<i32>(value * f) * i_f;
    }


    template <typename T>
    inline constexpr f32 sign_f32(T value)
    {           
        return sign<f32, T>(value);
    }


    template <typename T>
    inline constexpr i8 sign_i8(T value)
    {
        return sign<i8, T>(value);
    }


    template <typename T>
    inline constexpr T abs(T value)
    {
        return sign<T, T>(value) * value;
    }


    template <typename T>
    inline constexpr T min(T a, T b)
    {
        return a < b ? a : b;
    }


    template <typename T>
    inline constexpr T max(T a, T b)
    {
        return a > b ? a : b;
    }


    template <typename T>
    inline constexpr T min(T a, T b, T c, T d)
    {
        return min(min(a, b), min(c, d));
    }


    template <typename T>
    inline constexpr T max(T a, T b, T c, T d)
    {
        return max(max(a, b), max(c, d));
    }


    template <typename T>
    inline constexpr T cxpr_floor(T value)
    { 
        return (T)cxpr_round_to_signed<i64>(value - 0.5f);
    }


    template <typename T>
    inline constexpr T floor(T value)
    { 
        return (T)round_to_signed<i64>(value - 0.5f);
    }


    template <typename uT>
    inline constexpr uT unsigned_max()
    {
        static_assert(is_unsigned<uT>());

        return (uT)((uT)0 - (uT)1);
    }


    template <typename uT>
    inline constexpr uT scale_to_unsigned(f32 value)
    {
        static_assert(is_unsigned<uT>());

        constexpr f32 max = (f32)unsigned_max<uT>() + 1.0f;

        auto s = sign_f32(value);

        value -= s * (u64)value;
        value = s > 0.0f ? value : (1.0f - value);

        return round_to_unsigned<uT>(max * value);
    }


    template <typename uSrc, typename uDst>
    inline constexpr uDst scale_unsigned(uSrc value)
    {
        static_assert(is_unsigned<uSrc>());
        static_assert(is_unsigned<uDst>());

        constexpr f32 i_smax = 1.0f / ((f32)unsigned_max<uSrc>() + 1.0f);
        constexpr f32 dmax = (f32)unsigned_max<uDst>() + 1.0f;

        return round_to_unsigned<uDst>(dmax * value * i_smax);
    }


    template <typename T>
    inline constexpr T inc_wrap(T value, T min, T max)
    {
        ++value;
        return value < min || value > max ? min : value;
    }
}


namespace numeric
{
    inline f32 log(f32 x) 
    {
        u32 bx = *(u32*)(&x);
        u32 ex = bx >> 23;
        i32 t = (i32)ex-(i32)127;
        i32 s = (t < 0) ? (-t) : t;
        bx = 1065353216 | (bx & 8388607);
        x = *(f32*)(&bx);

        return -1.49278 + (2.11263 + (-0.729104+0.10969 * x) * x) * x + 0.6931471806 * t;
    }
    
    
    inline f32 q_rsqrt(f32 number)
    {
        long i;
        float x2, y;
        constexpr float threehalfs = 1.5F;

        x2 = number * 0.5F;
        y  = number;
        i  = * ( long * ) &y;
        i  = 0x5f3759df - ( i >> 1 );
        y  = * ( float * ) &i;
        y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
        // y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

        return y;
    }


    inline f32 q_sqrt(f32 number)
    {
        if (number <= 0.0f)
        {
            return 0.0f;
        }

        long i;
        float x2, y;
        constexpr float threehalfs = 1.5F;

        x2 = number * 0.5F;
        y  = number;
        i  = * ( long * ) &y;
        i  = 0x5f3759df - ( i >> 1 );
        y  = * ( float * ) &i;
        y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
        y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration

        return 1.0f / y;
    }


    inline f32 q_hypot(f32 a, f32 b)
    {
        return q_sqrt(a * a + b * b);
    }
}


/* vector */

namespace numeric
{
    template <typename T>
    inline f32 magnitude(Vec2D<T> const& vec)
    {
        auto const x = (f32)vec.x;
        auto const y = (f32)vec.y;

        return q_hypot(x, y);
        //return std::hypotf(x, y);
    }
}


/* trig */

namespace numeric
{
    static constexpr f64 PI = 3.14159265358979323846;


    template <typename T>
    inline constexpr T cxpr_sin_approx(T rad)
    {
        // best for small angles e.g. 0 - 45deg

        constexpr T B = (T)(4.0) / (T)(PI);
        constexpr T C = (T)(-4.0) / ((T)(PI * PI));
        constexpr T P = (T)(0.225);

        T y = B * rad + C * rad * abs(rad);
        y = P * (y * abs(y) - y) + y;

        return y;
    }


    template <typename T>
    inline constexpr T cxpr_cos_approx(T rad)
    {
        // best for small angles e.g. 0 - 45deg

        constexpr T tp = (T)(1.0) / (T)(2 * PI);

        T x = rad * tp;

        x -= (T)(0.25) + floor(x + T(0.25));
        x *= (T)(16.0) * (abs(x) - (T)(0.5));
        x += (T)(0.225) * x * (abs(x) - (T)(1.0));

        return x;
    }
    

    inline constexpr f32 sin_approx(f32 rad)
    {
        // best for small angles e.g. 0 - 45deg

        constexpr f32 B = 4.0f / (f32)PI;
        constexpr f32 C = -4.0f / ((f32)(PI * PI));
        constexpr f32 P = 0.225f;

        f32 y = fmaf(B, rad, C * rad * abs(rad));
        y = fmaf(P, fmaf(y, abs(y), -y), y);

        return y;
    }
    

    inline constexpr f32 cos_approx(f32 rad)
    {
        // best for small angles e.g. 0 - 45deg

        constexpr f32 tp = 1.0f / (f32)(2 * PI);

        f32 x = rad * tp;

        x -= 0.25f + floor(x + 0.25f);
        x *= 16.0f * (abs(x) - 0.5f);
        x += 0.225f * x * (abs(x) - 1.0f);

        return x;
    }


    inline constexpr f32 atan_approx(f32 tan)
    {
        f32 sq = tan * tan;

        constexpr f32 a1  =  0.99997726f;
        constexpr f32 a3  = -0.33262347f;
        constexpr f32 a5  =  0.19354346f;
        constexpr f32 a7  = -0.11643287f;
        constexpr f32 a9  =  0.05265332f;
        constexpr f32 a11 = -0.01172120f;        

        //return tan * (a1 + sq * (a3 + sq * (a5 + sq * (a7 + sq * (a9 + sq * a11)))));
        return tan * fmaf(sq, fmaf(sq, fmaf(sq, fmaf(sq, fmaf(sq, a11, a9), a7), a5), a3), a1);
    }


    template <typename T>
    inline constexpr T deg_to_rad(T deg)
    {
        constexpr auto scale = PI / 180.0;
        return (T)(deg * scale);
    }


    template <typename T>
    inline constexpr T rad_to_deg(T rad)
    {
        constexpr auto scale = 180.0 / PI;
        return (T)(rad * scale);
    }


    template <typename uT>
    inline constexpr f32 max_angle_f32()
    {
        static_assert(is_unsigned<uT>());

        return (f32)unsigned_max<uT>() + 1.0f;
    }


    template <typename uT>
    inline constexpr u64 max_angle_u64()
    {
        static_assert(is_unsigned<uT>());

        return (u64)unsigned_max<uT>() + 1u;
    }


    template <typename uT>
    inline constexpr f32 unsigned_to_rad(uT a)
    {
        static_assert(is_unsigned<uT>());

        constexpr f32 max = max_angle_f32<uT>();
        constexpr auto scale = 2 * PI / max;

        return (f32)(a * scale);
    }


    template <typename uT>
    inline constexpr uT rad_to_unsigned(f32 rad)
    {
        static_assert(is_unsigned<uT>());

        constexpr f32 TP = (f32)(2 * PI);
        constexpr f32 TP_I = (f32)1.0 / TP;

        rad = rad < 0.0 ? rad + TP : rad;
        rad = rad > TP ? rad - TP : rad;

        constexpr f32 max = max_angle_f32<uT>();

        return round_to_unsigned<uT>(max * rad * TP_I);
    }


    inline constexpr f32 u16_to_rad(u16 a)
    {
        return unsigned_to_rad(a);
    }


    inline constexpr f32 u8_to_rad(u8 a)
    {
        return unsigned_to_rad(a);
    }


    inline bool is_power_of_2(u64 num)
    {
        return (num && !(num & (num - 1)));
    }
}


/* sin cos uangle */

namespace numeric
{
    inline constexpr f32 sin(uangle a)
    {
        static_assert(sizeof(uangle) <= sizeof(u32));

        constexpr f32 P = (f32)PI;
        constexpr f32 TP = (f32)(2 * PI);
        constexpr f32 HP = (f32)(PI / 2);

        // split full rotation into 8 x 45deg sections
        constexpr u64 max = max_angle_u64<uangle>();
        constexpr u64 oct = max / 8;
        
        auto rad = unsigned_to_rad(a);

        switch (a / oct)
        {
            case 0: return sin_approx(rad);
            case 1: return cos_approx(HP - rad);
            case 2: return cos_approx(rad - HP);
            case 3: return sin_approx(P - rad);
            case 4: return -sin_approx(rad - P);
            case 5: return -cos_approx(P + HP - rad);
            case 6: return -cos_approx(rad - (P + HP));
            case 7: return -sin_approx(TP - rad);
            default: return 0.0f;
        }
    }


    inline constexpr f32 cos(uangle a)
    {
        static_assert(sizeof(uangle) <= sizeof(u32));

        constexpr f32 P = (f32)PI;
        constexpr f32 TP = (f32)(2 * PI);
        constexpr f32 HP = (f32)(PI / 2);

        // split full rotation into 8 x 45deg sections
        constexpr u64 max = max_angle_u64<uangle>();
        constexpr u64 oct = max / 8;
        
        auto rad = unsigned_to_rad(a);

        switch (a / oct)
        {
            case 0: return cos_approx(rad);
            case 1: return sin_approx(HP - rad);
            case 2: return -sin_approx(rad - HP);
            case 3: return -cos_approx(P - rad);
            case 4: return -cos_approx(rad - P);
            case 5: return -sin_approx(P + HP - rad);
            case 6: return sin_approx(rad - (P + HP));
            case 7: return cos_approx(TP - rad);
            default: return 0.0f;
        }
    }


    inline constexpr uangle atan2(f32 sin, f32 cos)
    { 
        constexpr f32 P = (f32)PI;
        constexpr f32 TP = (f32)(2 * PI);
        constexpr f32 HP = (f32)(PI / 2);
        constexpr f32 QP = (f32)(PI / 4);

        assert(abs((cos * cos + sin * sin) - 1.0f) < 0.001f);

        auto pcos = abs(cos);
        auto psin = abs(sin);

        auto flip_45 = pcos < psin;
        auto flip_y = cos < 0.0f;
        auto flip_x = sin < 0.0f;

        auto key_y = (int)flip_y << 2;
        auto key_x = (int)flip_x << 1;
        auto key_45 = (int)flip_45;

        int oct_key = key_y | key_x | key_45;

        auto tan = flip_45 ? pcos / psin : psin / pcos;
        auto rad = atan_approx(tan);
        
        assert(rad >= 0.0f);
        assert(rad <= QP);

        switch (oct_key)
        {
        case 0b000:
            // octant 0
            break;

        case 0b001:
            rad = HP - rad; // octant 1
            break;

        case 0b101:
            rad = HP + rad; // octant 2
            break;

        case 0b100:
            rad = P - rad; // octant 3
            break;

        case 0b110:
            rad = P + rad; // octant 4
            break;

        case 0b111:
            rad = 3 * HP - rad; // octant 5
            break;

        case 0b011:
            rad = 3 * HP + rad; // octant 6
            break;

        case 0b010:
            rad = TP - rad; // octant 7
            break;
        
        default:
            break;
        }

        return rad_to_unsigned<uangle>(rad);
    }
}