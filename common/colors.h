#ifndef COLORS_H
#define COLORS_H

// NOTE: 'colormap' is really hue (HSV/HSL color space) but HL uses 1 byte (0...255) range instead of degrees (0...360)

#define COLORMAP_RED		0x0000// 0
#define COLORMAP_ORANGE		0x1414// 20
#define COLORMAP_YELLOW		0x2828// 40
#define COLORMAP_GREEN		0x5050// 80
#define COLORMAP_CYAN		0x7878// 120
#define COLORMAP_BLUE		0xA0A0// 160
#define COLORMAP_VIOL		0xC8C8// 200

// The following "colors" are palette indexes (palette.lmp)
#ifndef BLOOD_COLOR_RED
#define	BLOOD_COLOR_RED			247
#endif

#ifndef BLOOD_COLOR_YELLOW
#define	BLOOD_COLOR_YELLOW		192
#endif

#ifndef BLOOD_COLOR_GREEN
#define	BLOOD_COLOR_GREEN		216
#endif

#ifndef BLOOD_COLOR_BLUE
#define	BLOOD_COLOR_BLUE		208
#endif

#define	BLOOD_COLOR_BLACK	0//(0,0,0)
#define	BLOOD_COLOR_GRAY	8//(123,123,123)
#define	BLOOD_COLOR_MAGENTA	144//(186,116,160)

#define	BLOOD_COLOR_YELLOW1	195//(204,188,0)
#define	BLOOD_COLOR_YELLOW2	200//(120,100,0)
#define	BLOOD_COLOR_YELLOW3	203//(76,56,0)

#define	BLOOD_COLOR_BLUE1	209//(0,0,238)
#define	BLOOD_COLOR_BLUE2	210//(0,0,220)
#define	BLOOD_COLOR_BLUE3	211//(0,0,204)
#define	BLOOD_COLOR_BLUE4	212//(0,0,186)
#define	BLOOD_COLOR_BLUE5	213//(0,0,170)
#define	BLOOD_COLOR_BLUE6	214//(0,0,154)
#define	BLOOD_COLOR_BLUE7	215//(0,0,136)

#define	BLOOD_COLOR_GREEN1	217//(0,238,0)
#define	BLOOD_COLOR_GREEN2	218//(0,220,0)
#define	BLOOD_COLOR_GREEN3	219//(0,204,0)
#define	BLOOD_COLOR_GREEN4	220//(0,186,0)
#define	BLOOD_COLOR_GREEN5	221//(0,170,0)
#define	BLOOD_COLOR_GREEN6	222//(0,154,0)
#define	BLOOD_COLOR_GREEN7	223//(0,136,0)

#define	BLOOD_COLOR_HUM_SKN	239//(246,210,140)
#define	BLOOD_COLOR_CYAN	245//(160,236,255)

#define	BLOOD_COLOR_RED1	248//(140,0,0)
#define	BLOOD_COLOR_RED2	249//(180,0,0)
#define	BLOOD_COLOR_RED3	250//(216,0,0)
#define	BLOOD_COLOR_RED4	251//(255,0,0)

#define	BLOOD_COLOR_WHITE	254//(255,255,255)

#endif // COLORS_H