#include <gtk/gtk.h>
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// --- CONFIGURATION USB ---
#define CASIO_VENDOR_ID 0x07cf

// Commandes de déverrouillage
unsigned char cmd1[] = {0x05, 0x30, 0x30, 0x30, 0x37, 0x30};
unsigned char cmd2[] = {0x18, 0x30, 0x30, 0x30, 0x37, 0x30};

// Widgets globaux
GtkWidget *status_label;
GtkWidget *window;

// --- LOGIQUE BACKEND (LIBUSB) ---

// Fonction de scan pour l'affichage (ne connecte pas, juste liste)
int scan_devices(char *output_buffer) {
    libusb_context *ctx = NULL;
    libusb_device **devs;
    ssize_t cnt;
    int found = 0;

    if (libusb_init(&ctx) < 0) {
        sprintf(output_buffer, "Erreur critique: Init Libusb");
        return -1;
    }

    cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        sprintf(output_buffer, "Erreur scan USB");
        libusb_exit(ctx);
        return -1;
    }

    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(devs[i], &desc);
        if (desc.idVendor == CASIO_VENDOR_ID) {
            sprintf(output_buffer, "Connecté: Casio ID %04x:%04x", desc.idVendor, desc.idProduct);
            found = 1;
            break;
        }
    }

    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);

    if (!found) {
        sprintf(output_buffer, "Aucune calculatrice détectée.");
        return 0;
    }
    return 1;
}

// Fonction principale de déverrouillage
void perform_unlock() {
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle = NULL;
    libusb_device **devs;
    libusb_device *dev = NULL;
    int r, actual;
    
    // Variables dynamiques pour les endpoints
    int ep_out = -1, ep_in = -1, ep_intr = -1;

    // Feedback visuel immédiat
    gtk_label_set_text(GTK_LABEL(status_label), "Traitement en cours...");
    while (gtk_events_pending()) gtk_main_iteration(); 

    libusb_init(&ctx);
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    
    // Recherche du device
    for (int i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(devs[i], &desc);
        if (desc.idVendor == CASIO_VENDOR_ID) {
            dev = devs[i];
            libusb_open(dev, &dev_handle);
            break; 
        }
    }

    if (dev_handle == NULL) {
        gtk_label_set_text(GTK_LABEL(status_label), "Erreur: Impossible d'ouvrir (sudo requis?)");
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return;
    }

    // Scan des Endpoints (IN/OUT/INTERRUPT)
    struct libusb_config_descriptor *config;
    libusb_get_active_config_descriptor(dev, &config);
    if (!config) libusb_get_config_descriptor(dev, 0, &config);

    if (config) {
        for (int j = 0; j < config->bNumInterfaces; j++) {
            const struct libusb_interface *inter = &config->interface[j];
            for (int k = 0; k < inter->num_altsetting; k++) {
                const struct libusb_interface_descriptor *interdesc = &inter->altsetting[k];
                for (int l = 0; l < interdesc->bNumEndpoints; l++) {
                    const struct libusb_endpoint_descriptor *epdesc = &interdesc->endpoint[l];
                    
                    // Bulk OUT (Ecriture)
                    if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                        if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
                            if (ep_out == -1) ep_out = epdesc->bEndpointAddress;
                    }
                    // Bulk IN (Lecture)
                    if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                        if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
                            if (ep_in == -1) ep_in = epdesc->bEndpointAddress;
                    }
                    // Interrupt IN
                    if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                         if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
                            ep_intr = epdesc->bEndpointAddress;
                    }
                }
            }
        }
        libusb_free_config_descriptor(config);
    }
    libusb_free_device_list(devs, 1);

    if (ep_out == -1) {
        gtk_label_set_text(GTK_LABEL(status_label), "Erreur: Endpoints introuvables.");
        goto exit_unlock;
    }

    // Détachement du pilote noyau (Crucial pour Linux)
    if(libusb_kernel_driver_active(dev_handle, 0) == 1)
        libusb_detach_kernel_driver(dev_handle, 0);

    libusb_claim_interface(dev_handle, 0);
    
    // Nettoyage préventif
    libusb_clear_halt(dev_handle, ep_out);
    if(ep_in != -1) libusb_clear_halt(dev_handle, ep_in);

    // 1. Envoi Control Init
    libusb_control_transfer(dev_handle, 0x41, 0x01, 0, 0, NULL, 0, 1000);

    unsigned char buf[512];
    // Vidange interruption
    if (ep_intr != -1) libusb_interrupt_transfer(dev_handle, ep_intr, buf, sizeof(buf), &actual, 50);

    // 2. Envoi Commande 1 + ZLP + Lecture réponse
    r = libusb_bulk_transfer(dev_handle, ep_out, cmd1, sizeof(cmd1), &actual, 2000);
    if(r == 0) {
        libusb_bulk_transfer(dev_handle, ep_out, NULL, 0, &actual, 1000); // ZLP
        
        // Lecture de la réponse (Indispensable pour éviter le blocage sur certains modèles)
        if (ep_in != -1) {
            libusb_bulk_transfer(dev_handle, ep_in, buf, sizeof(buf), &actual, 1500);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "Erreur: Echec envoi commande 1");
        goto exit_unlock;
    }

    usleep(200000); // Pause 0.2s

    // 3. Envoi Commande 2 + ZLP
    r = libusb_bulk_transfer(dev_handle, ep_out, cmd2, sizeof(cmd2), &actual, 2000);
    if(r == 0) {
        libusb_bulk_transfer(dev_handle, ep_out, NULL, 0, &actual, 1000);
        
        // Tentative lecture confirmation (optionnel mais propre)
        if (ep_in != -1) libusb_bulk_transfer(dev_handle, ep_in, buf, sizeof(buf), &actual, 500);

        gtk_label_set_text(GTK_LABEL(status_label), "SUCCÈS : Mode Examen désactivé !");
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "Erreur: Echec envoi confirmation");
    }

exit_unlock:
    if(dev_handle) {
        libusb_release_interface(dev_handle, 0);
        libusb_close(dev_handle);
    }
    libusb_exit(ctx);
}

// --- INTERFACE GRAPHIQUE (GTK) ---

void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    char buffer[256];
    scan_devices(buffer);
    gtk_label_set_text(GTK_LABEL(status_label), buffer);
}

void on_unlock_clicked(GtkWidget *widget, gpointer data) {
    perform_unlock();
}

void on_info_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *img_old, *img_new;
    GtkWidget *lbl_title_old, *lbl_desc_old, *lbl_steps_old;
    GtkWidget *lbl_title_new, *lbl_steps_new, *lbl_compat_new;
    GdkPixbuf *pixbuf;

    dialog = gtk_dialog_new_with_buttons("Plus d'informations", GTK_WINDOW(window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "Fermer", GTK_RESPONSE_CLOSE, NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 750, 500);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    // --- COLONNE 1 : Graph 35+E (Vieux) ---
    lbl_title_old = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title_old), "<b>GRAPH 35+E (Ancien)</b>");
    
    pixbuf = gdk_pixbuf_new_from_file_at_scale("Images/graph35+e.jpeg", 200, -1, TRUE, NULL);
    if (pixbuf) {
        img_old = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
    } else {
        img_old = gtk_label_new("[Image non trouvée]");
    }

    lbl_desc_old = gtk_label_new("Ce logiciel est conçu pour\ndésactiver le mode examen\nsur ce modèle via USB.\n\n"
                                 "Cette manipulation est\nnormalement réservée aux\npériphériques Windows ou MacOS,\n"
                                 "mais cette application permet\nd'enlever le mode examen.");
    gtk_label_set_justify(GTK_LABEL(lbl_desc_old), GTK_JUSTIFY_CENTER);

    lbl_steps_old = gtk_label_new("Instructions pour ce modèle :\n\n"
                                  "1. Brancher calculatrice à l'ordinateur\n"
                                  "2. Appuyer sur F1 (TransDon)\n"
                                  "3. Scanner avec l'application\n"
                                  "4. Cliquer sur 'DÉCONNECTER'\n"
                                  "5. Mode examen enlevé, appuyer sur Exit");
    gtk_label_set_xalign(GTK_LABEL(lbl_steps_old), 0.0);

    gtk_grid_attach(GTK_GRID(grid), lbl_title_old, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), img_old, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_desc_old, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_steps_old, 0, 3, 1, 1);


    // --- COLONNE 2 : Graph 35+E II ---
    lbl_title_new = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title_new), "<b>GRAPH 35+E II (Nouveau)</b>");

    pixbuf = gdk_pixbuf_new_from_file_at_scale("Images/graph35+eII.jpeg", 200, -1, TRUE, NULL);
    if (pixbuf) {
        img_new = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
    } else {
        img_new = gtk_label_new("[Image non trouvée]");
    }

    // Info compatibilité spécifique à la colonne 2
    lbl_compat_new = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_compat_new), "<span foreground='#2E8B57' size='small'><b>✅ Compatible tous systèmes d'exploitation</b></span>");
    
    lbl_steps_new = gtk_label_new("Étapes manuelles (Alternative) :\n\n"
                                  "1. Brancher calculatrice à l'ordinateur\n"
                                  "2. Appuyer sur F1 (Choisir Clé USB)\n"
                                  "3. Créer une modification dans le dossier\n   (ex: créer un fichier vide.txt)\n"
                                  "4. Éjecter le périphérique USB\n"
                                  "5. Débrancher le câble\n"
                                  "6. Mode examen retiré, appuyer sur Exit");
    gtk_label_set_xalign(GTK_LABEL(lbl_steps_new), 0.0);

    gtk_grid_attach(GTK_GRID(grid), lbl_title_new, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), img_new, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_compat_new, 1, 2, 1, 1); 
    gtk_grid_attach(GTK_GRID(grid), lbl_steps_new, 1, 3, 1, 1);  

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    GtkWidget *grid;
    GtkWidget *btn_refresh, *btn_unlock, *btn_info;
    GtkWidget *lbl_header, *lbl_warning, *img_logo;
    GdkPixbuf *logo_pixbuf;
    GError *error = NULL;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Casio Unlocker Linux");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 450); // Ajusté pour le contenu
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Chargement de l'icône de la fenêtre
    if(!gtk_window_set_icon_from_file(GTK_WINDOW(window), "Images/logo.png", &error)) {
        g_warning("Attention: Icône non chargée: %s", error->message);
        g_error_free(error);
    }

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);

    // Logo dans l'interface
    logo_pixbuf = gdk_pixbuf_new_from_file_at_scale("Images/logo.png", 64, 64, TRUE, NULL);
    if(logo_pixbuf) {
        img_logo = gtk_image_new_from_pixbuf(logo_pixbuf);
        gtk_grid_attach(GTK_GRID(grid), img_logo, 0, 0, 2, 1);
    }

    lbl_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_header), "<span size='x-large' weight='bold'>Unlocker GRAPH 35+E</span>");
    gtk_grid_attach(GTK_GRID(grid), lbl_header, 0, 1, 2, 1);

    lbl_warning = gtk_label_new("Logiciel non officiel. Utilisation à vos risques.");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_warning), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), lbl_warning, 0, 2, 2, 1);

    status_label = gtk_label_new("En attente de connexion...");
    gtk_label_set_ellipsize(GTK_LABEL(status_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(status_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 3, 2, 1);

    btn_refresh = gtk_button_new_with_label("Rafraîchir / Scanner");
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_refresh, 0, 4, 1, 1);

    btn_info = gtk_button_new_with_label("Plus d'infos");
    g_signal_connect(btn_info, "clicked", G_CALLBACK(on_info_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_info, 1, 4, 1, 1);

    btn_unlock = gtk_button_new_with_label("DÉCONNECTER (Retirer Mode Examen)");
    GtkStyleContext *context = gtk_widget_get_style_context(btn_unlock);
    gtk_style_context_add_class(context, "suggested-action");
    g_signal_connect(btn_unlock, "clicked", G_CALLBACK(on_unlock_clicked), NULL);
    gtk_widget_set_size_request(btn_unlock, -1, 50);
    gtk_grid_attach(GTK_GRID(grid), btn_unlock, 0, 5, 2, 1);

    char buffer[256];
    scan_devices(buffer);
    gtk_label_set_text(GTK_LABEL(status_label), buffer);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}