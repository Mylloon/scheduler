/*!\file images_io.c
 *
 * \brief lecture et sauvegarde d'images dans différents formats (à
 * compléter selon le besoin).
 *
 * \author Farès Belhadj amsi@up8.edu
 * \date March 02, 2020
 */
#define _CRT_SECURE_NO_WARNINGS // pour les warning VS sur fopen
#include "../includes/images_io.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*!\brief Inversion des couleurs dans les fichiers BMP : bgr -> rgb et
 * inversement.
 */
static void
bgr2rgb(unsigned char *data, int size)
{
    int i;
    unsigned char c;
    for(i = 0; i < size; i += 3) {
        c = data[i];
        data[i] = data[i + 2];
        data[i + 2] = c;
    }
}

/* \brief permet de sauvegarder une image au format BPM.
 *
 * ne gère que des BMP à une couche (niveaux de gris) ou 3 couches
 * (RGB) à 8 bits par composante ; tout autre configuration donnera
 * lieu à un échec d'assertion.
 * \param file_name nom du fichier destination
 * \param data donnée de l'image
 * \param w largeur de l'image
 * \param h hauteur de l'image
 * \param Bpp nombre de Bytes par pixel (1 ou 3)
 * \param bit_depth nombre de bits pour chaque composante de pixel
 * \return 0 si succès, sinon un code erreur lié à la création du
 * fichier (errno).
 */
int
imageSaveBMP(const char *file_name, unsigned char *data, int w, int h, int Bpp,
             int bit_depth)
{
    FILE *fp;
    unsigned char sig[4] = "BM", *tdata;
    int i, comp = 0, fsize, isize, lsize, hres = 3420, vres = 3388;
    int res = 0, offset = 54, ihead = 0x28, plans = 1, bpp, cpal = 0;
    sig[3] = 0;
    assert(Bpp == 1 || Bpp == 3);
    assert(bit_depth == 8);
    if((fp = fopen(file_name, "wb")) == NULL) {
        fprintf(stderr, "%s(%d): errno %d : %s\n", __FILE__, __LINE__, errno,
                strerror(errno));
        return errno;
    }
    lsize = Bpp * w;
    lsize += (lsize % 4) ? (4 - (lsize % 4)) : 0;
    isize = lsize * h;
    offset += Bpp == 1 ? 1024 : 0;
    fsize = isize + offset;
    bpp = bit_depth * Bpp;
    fwrite(sig, 1, 2, fp);
    fwrite(&fsize, 4, 1, fp);
    fwrite(&res, 4, 1, fp);
    fwrite(&offset, 4, 1, fp);
    fwrite(&ihead, 4, 1, fp);
    fwrite(&w, 4, 1, fp);
    fwrite(&h, 4, 1, fp);
    fwrite(&plans, 2, 1, fp);
    fwrite(&bpp, 2, 1, fp);
    fwrite(&comp, 4, 1, fp);
    fwrite(&isize, 4, 1, fp);
    fwrite(&hres, 4, 1, fp);
    fwrite(&vres, 4, 1, fp);
    if(Bpp == 3) {
        fwrite(&cpal, 4, 1, fp);
        fwrite(&cpal, 4, 1, fp);
    } else {
        cpal = 256;
        fwrite(&cpal, 4, 1, fp);
        fwrite(&cpal, 4, 1, fp);
        for(i = 0, sig[3] = 0; i < 256; i++) {
            sig[0] = sig[1] = sig[2] = i;
            fwrite(sig, 1, 4, fp);
        }
    }
    tdata = calloc(lsize, sizeof *tdata);
    assert(tdata);
    for(i = 0; i < h; i++) {
        memcpy(tdata, &data[(h - i - 1) * Bpp * w], Bpp * w * sizeof *tdata);
        if(Bpp == 3) {
            bgr2rgb(tdata, Bpp * w);
        }
        fwrite(tdata, 1, lsize, fp);
    }
    free(tdata);
    fclose(fp);
    return 0;
}
