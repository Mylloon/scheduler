/*!\file images_io.h
 *
 * \brief lecture et sauvegarde d'images dans différents formats (à
 * compléter selon le besoin).
 *
 * \author Farès Belhadj amsi@up8.edu
 * \date March 02, 2020
 */
#ifndef _IMAGES_IO_H
#define _IMAGES_IO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int imageSaveBMP(const char *file_name, unsigned char *data, int w,
                        int h, int Bpp, int bit_depth);

#ifdef __cplusplus
}
#endif

#endif
