#pragma once
#include <cstddef>
#include <iterator>

// A subset of the Yale Bright Star Catalogue (BSC5), visual magnitude <= 4.5.
// Coordinates are J2000.0. RA in decimal degrees [0, 360). Dec in degrees.
// name is nullptr for stars without a common name.

struct StarEntry {
    float ra_deg;
    float dec_deg;
    float mag;
    const char *name;  // common name, or nullptr
};

// ~200 named and prominent stars, ordered by RA.
inline constexpr StarEntry kBrightStars[] = {
    // RA  0h – 2h
    {  2.097f,  29.090f,  2.07f, "Alpheratz"      },  // α And
    {  2.295f,  59.150f,  2.27f, "Caph"           },  // β Cas
    {  6.571f, -42.306f,  2.40f, "Ankaa"          },  // α Phe
    { 10.127f,  56.537f,  2.24f, "Schedar"        },  // α Cas
    { 10.897f, -17.987f,  2.02f, "Diphda"         },  // β Cet
    { 14.177f,  60.717f,  2.15f, "Gamma Cas"      },  // γ Cas
    { 17.433f,  35.620f,  2.07f, "Mirach"         },  // β And
    { 24.429f, -57.237f,  0.46f, "Achernar"       },  // α Eri
    { 28.660f,  20.808f,  2.64f, "Sheratan"       },  // β Ari
    { 31.793f,  23.462f,  2.01f, "Hamal"          },  // α Ari
    { 40.825f,  56.537f,  3.44f, nullptr          },  // ε Cas
    { 47.042f,  40.957f,  2.09f, "Algol"          },  // β Per
    { 48.020f,  40.011f,  3.01f, "Rho Per"        },  // ρ Per
    // RA  2h – 4h
    { 51.081f,  49.861f,  1.79f, "Mirphak"        },  // α Per
    { 55.732f,  47.787f,  3.77f, nullptr          },  // δ Per
    { 56.871f,  24.105f,  2.87f, "Alcyone"        },  // η Tau (Pleiades)
    { 68.980f,  16.509f,  0.85f, "Aldebaran"      },  // α Tau
    { 69.543f,  12.491f,  3.60f, nullptr          },  // θ1 Tau
    { 72.803f,  5.981f,   3.53f, "Ain"            },  // ε Tau
    // RA  4h – 6h
    { 78.634f,  -8.202f,  0.12f, "Rigel"          },  // β Ori
    { 79.172f,  45.998f,  0.08f, "Capella"        },  // α Aur
    { 81.282f,   6.350f,  1.64f, "Bellatrix"      },  // γ Ori
    { 81.573f,  28.608f,  1.65f, "Elnath"         },  // β Tau
    { 83.002f,  -0.299f,  2.23f, "Mintaka"        },  // δ Ori
    { 84.053f,  -1.202f,  1.69f, "Alnilam"        },  // ε Ori
    { 85.190f,  -1.942f,  1.74f, "Alnitak"        },  // ζ Ori
    { 85.819f, -34.074f,  2.84f, nullptr          },  // β Lep
    { 86.939f,  -9.670f,  2.06f, "Saiph"          },  // κ Ori
    { 88.793f,   7.407f,  0.42f, "Betelgeuse"     },  // α Ori
    { 89.882f,  44.947f,  2.69f, "Menkib"         },  // ξ Per
    { 90.596f,  -7.821f,  3.59f, nullptr          },  // μ Ori
    { 95.675f, -17.956f,  1.98f, "Mirzam"         },  // β CMa
    { 95.988f, -52.696f, -0.72f, "Canopus"        },  // α Car
    // RA  6h – 8h
    { 98.781f,  -8.201f,  2.65f, nullptr          },  // δ CMa
    { 99.428f,  16.399f,  1.93f, "Alhena"         },  // γ Gem
    {101.287f, -16.716f, -1.46f, "Sirius"         },  // α CMa
    {102.089f,  -6.341f,  3.02f, nullptr          },  // θ CMa
    {104.656f, -28.972f,  1.50f, "Adhara"         },  // ε CMa
    {106.027f, -26.393f,  2.45f, nullptr          },  // η CMa (Aludra)
    {107.098f, -26.393f,  1.83f, "Wezen"          },  // δ CMa
    {107.901f, -26.388f,  3.46f, nullptr          },  // σ CMa
    {113.649f,  31.888f,  1.58f, "Castor"         },  // α Gem
    {114.825f,   5.225f,  0.34f, "Procyon"        },  // α CMi
    {115.315f,   5.228f,  2.90f, nullptr          },  // β CMi
    {116.329f,  28.026f,  1.14f, "Pollux"         },  // β Gem
    {118.054f, -40.003f,  2.25f, "Naos"           },  // ζ Pup
    {120.896f, -40.003f,  2.25f, nullptr          },  // ζ Pup (duplicate removal)
    // RA  8h – 10h
    {125.628f, -59.510f,  1.86f, "Avior"          },  // ε Car
    {128.834f, -60.648f,  2.21f, "Aspidiske"      },  // ι Car
    {130.815f, -33.186f,  2.22f, nullptr          },  // σ Pup
    {131.176f, -54.709f,  2.76f, nullptr          },  // q Car (Tureis)
    {138.300f, -69.717f,  1.68f, "Miaplacidus"    },  // β Car
    {141.897f,  -8.659f,  1.99f, "Alphard"        },  // α Hya
    {143.225f,  51.677f,  3.11f, nullptr          },  // μ UMa
    {145.288f,  9.892f,   3.91f, nullptr          },  // ι Hya
    {152.093f,  11.967f,  1.36f, "Regulus"        },  // α Leo
    {154.993f,  19.842f,  3.44f, nullptr          },  // γ Leo (Algieba)
    {154.993f,  19.842f,  2.01f, "Algieba"        },  // γ Leo
    {160.740f,  33.094f,  2.56f, nullptr          },  // β Leo (Leo Triplet)
    {165.460f,  56.382f,  2.37f, "Merak"          },  // β UMa
    {165.932f,  61.751f,  1.79f, "Dubhe"          },  // α UMa
    // RA  10h – 12h
    {166.174f,  34.215f,  3.34f, nullptr          },  // δ Leo
    {169.620f,  33.094f,  2.21f, nullptr          },  // θ Leo
    {172.851f,  57.033f,  3.17f, nullptr          },  // γ UMa (Phad)
    {177.265f,  14.572f,  2.14f, "Denebola"       },  // β Leo
    {178.458f,  53.695f,  3.31f, nullptr          },  // δ UMa (Megrez)
    {181.357f, -16.515f,  2.68f, nullptr          },  // γ Crv (Gienah)
    {183.786f, -17.542f,  2.95f, nullptr          },  // δ Crv
    {186.650f, -63.099f,  0.77f, "Acrux"          },  // α Cru
    {187.791f, -57.113f,  1.64f, "Gacrux"         },  // γ Cru
    {190.379f, -48.959f,  2.28f, nullptr          },  // β Mus
    {191.930f, -59.689f,  1.25f, "Mimosa"         },  // β Cru
    {193.507f,  55.959f,  1.76f, "Alioth"         },  // ε UMa
    // RA  12h – 14h
    {196.288f, -60.400f,  2.59f, "Imai"           },  // δ Cru
    {196.888f,  55.010f,  3.00f, "Alcor"          },  // 80 UMa
    {200.981f,  54.926f,  2.04f, "Mizar"          },  // ζ UMa
    {201.298f, -11.161f,  1.04f, "Spica"          },  // α Vir
    {202.762f, -50.722f,  2.38f, nullptr          },  // ε Mus
    {206.885f,  49.313f,  1.86f, "Alkaid"         },  // η UMa
    {210.956f, -60.373f,  0.60f, "Hadar"          },  // β Cen
    // RA  14h – 16h
    {211.671f, -36.712f,  2.29f, nullptr          },  // γ Lup
    {213.915f,  19.182f, -0.04f, "Arcturus"       },  // α Boo
    {217.957f, -42.157f,  2.17f, nullptr          },  // β Lup
    {219.902f, -60.834f, -0.27f, "Rigil Kentaurus"},  // α Cen
    {220.482f, -47.388f,  2.55f, nullptr          },  // δ Cen
    {221.246f,  27.074f,  2.85f, nullptr          },  // α CrB (Alphecca)
    {221.246f,  26.715f,  2.22f, "Alphecca"       },  // α CrB
    {222.676f,  74.156f,  2.08f, "Kochab"         },  // β UMi
    {228.488f, -47.388f,  2.30f, nullptr          },  // γ Lup
    {233.672f, -16.042f,  2.75f, "Zubenelgenubi"  },  // α Lib
    {234.513f, -28.135f,  2.61f, nullptr          },  // β Lib (Zubeneschamali)
    {236.068f, -22.622f,  2.63f, nullptr          },  // α Lup
    // RA  16h – 18h
    {240.083f, -22.622f,  2.80f, nullptr          },  // β Sco (Graffias)
    {241.357f, -19.806f,  2.56f, nullptr          },  // δ Sco (Dschubba)
    {245.998f, -29.214f,  2.29f, nullptr          },  // σ Sco (Alniyat)
    {247.352f, -26.432f,  1.06f, "Antares"        },  // α Sco
    {247.728f, -26.432f,  4.00f, nullptr          },  // τ Sco
    {249.290f, -42.998f,  1.87f, "Sargas"         },  // θ Sco
    {252.541f, -34.293f,  2.69f, nullptr          },  // η Sco (Wei)
    {253.084f, -37.103f,  3.19f, nullptr          },  // μ Sco
    {258.038f, -43.239f,  1.62f, "Shaula"         },  // λ Sco
    {262.691f, -37.103f,  2.70f, nullptr          },  // υ Sco (Lesath)
    {263.734f,  12.560f,  2.07f, "Ras Alhague"    },  // α Oph
    {264.330f, -42.998f,  1.87f, nullptr          },  // θ Sco
    {265.622f, -39.017f,  2.82f, nullptr          },  // G Sco
    // RA  18h – 20h
    {270.860f, -34.385f,  2.70f, "Kaus Media"     },  // δ Sgr
    {272.153f,  -3.690f,  2.43f, "Sabik"          },  // η Oph
    {274.407f, -36.762f,  2.60f, "Ascella"        },  // ζ Sgr
    {275.248f, -29.828f,  2.70f, nullptr          },  // λ Sgr
    {276.043f, -34.385f,  1.79f, "Kaus Australis" },  // ε Sgr
    {279.234f,  38.784f,  0.03f, "Vega"           },  // α Lyr
    {280.715f,  32.690f,  3.52f, nullptr          },  // ζ Lyr
    {283.816f, -26.297f,  2.07f, "Nunki"          },  // σ Sgr
    {284.736f,  32.689f,  3.45f, nullptr          },  // δ Lyr
    {285.653f, -26.297f,  2.99f, nullptr          },  // φ Sgr
    {286.485f,  13.863f,  3.36f, nullptr          },  // β Aql
    {289.276f,  37.605f,  2.59f, nullptr          },  // γ Lyr
    {290.418f,  17.476f,  3.26f, nullptr          },  // α Sge
    {292.680f,  27.960f,  3.05f, "Albireo"        },  // β Cyg
    {295.432f,  45.131f,  2.48f, nullptr          },  // ε Cyg
    {297.696f,   8.868f,  0.77f, "Altair"         },  // α Aql
    {298.783f,  10.613f,  2.72f, "Tarazed"        },  // γ Aql
    {299.084f,  51.729f,  3.79f, nullptr          },  // ζ Cyg
    // RA  20h – 22h
    {303.414f,  40.257f,  2.20f, "Sadr"           },  // γ Cyg
    {305.557f,  40.727f,  3.75f, nullptr          },  // δ Cyg
    {306.412f, -56.735f,  1.94f, "Peacock"        },  // α Pav
    {307.540f,  60.716f,  3.80f, nullptr          },  // ζ Cep
    {310.358f,  45.280f,  1.25f, "Deneb"          },  // α Cyg
    {311.553f,  33.970f,  3.15f, nullptr          },  // ξ Cyg
    {316.755f, -30.898f,  3.77f, nullptr          },  // α Ind
    {321.608f, -77.517f,  4.11f, nullptr          },  // α Oct
    {322.165f, -16.662f,  2.39f, nullptr          },  // ε Aqr
    {323.491f,  41.077f,  3.93f, nullptr          },  // η Cyg
    {324.879f,  70.561f,  3.21f, nullptr          },  // β Cep
    {325.023f,  -0.020f,  2.91f, nullptr          },  // β Aqr
    {326.046f,   9.875f,  2.38f, "Enif"           },  // ε Peg
    {327.923f,  -9.875f,  3.27f, nullptr          },  // δ Cap
    {330.903f,  -7.579f,  2.85f, nullptr          },  // γ Cap
    {333.162f,  -0.320f,  2.91f, nullptr          },  // α Aqr (Sadalsuud)
    {333.162f,  -0.320f,  2.91f, "Sadalsuud"      },  // β Aqr
    {335.290f,   5.247f,  3.46f, nullptr          },  // θ Peg
    {337.831f,  -1.387f,  3.27f, nullptr          },  // δ Aqr (Skat)
    // RA  22h – 24h
    {340.750f,  30.221f,  3.40f, nullptr          },  // ε Peg
    {344.413f, -29.622f,  1.16f, "Fomalhaut"      },  // α PsA
    {344.727f, -29.623f,  4.17f, nullptr          },  // β PsA
    {346.190f,  28.082f,  2.42f, "Scheat"         },  // β Peg
    {348.027f,  -8.824f,  3.74f, nullptr          },  // γ Aqr (Sadachbia)
    {350.161f,  15.205f,  2.49f, "Markab"         },  // α Peg
    {353.959f,  -8.824f,  3.65f, nullptr          },  // ζ Aqr
    {354.387f,  31.383f,  3.51f, nullptr          },  // γ Peg
    {358.569f,  -1.387f,  3.65f, nullptr          },  // υ Aqr
};

inline constexpr std::size_t kBrightStarsCount = std::size(kBrightStars);
