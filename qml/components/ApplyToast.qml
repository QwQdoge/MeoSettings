import QtQuick
import QtQuick.Controls
import MeoUI

// One-line confirmation shown after a setting is applied, so pages with an
// explicit Apply button give the same feedback as pages that apply instantly.
MeoSnackbar {
    id: toast
    parent: Overlay.overlay
    icon: "check_circle"
    duration: 2600

    function show(text) {
        toast.message = text
        toast.open()
    }
}
