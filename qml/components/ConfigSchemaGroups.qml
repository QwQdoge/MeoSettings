import QtQuick
import MeoUI

// The grouped controls for one KdeConfigBackend schema page. Shared by the
// dedicated schema page and by pages (Appearance) that host several schemas.
Column {
    id: groups

    property string schemaId: ""
    readonly property var sections: {
        KdeConfigBackend.revision
        return KdeConfigBackend.schema(schemaId)
    }
    readonly property bool hasContent: sections.length > 0
    signal applied(string message)

    spacing: MeoTheme.space16

    // Sliders report every step while dragging; commit once the user pauses.
    property var pending: ({})
    function queueNumber(rowId, value) {
        const next = Object.assign({}, pending)
        next[rowId] = value
        pending = next
        commitTimer.restart()
    }
    Timer {
        id: commitTimer
        interval: 350
        onTriggered: {
            const batch = groups.pending
            groups.pending = ({})
            for (const rowId in batch)
                KdeConfigBackend.setValue(groups.schemaId, rowId, batch[rowId])
            groups.applied(qsTr("Setting applied"))
        }
    }

    function rowsFor(section) {
        return section.rows.map(function(row) {
            const item = { "id": row.id, "title": row.label, "subtitle": row.description || "" }
            if (row.type === "toggle") {
                item.trailingKind = "toggle"
                item.checked = row.value === true
            } else if (row.type === "number") {
                item.trailingKind = "slider"
                item.from = row.min
                item.to = row.max
                item.stepSize = row.step
                item.value = row.value
                item.valueSuffix = row.suffix ? " " + row.suffix : ""
            } else {
                item.trailingKind = "dropdown"
                item.options = row.choices
                let selected = -1
                for (let i = 0; i < row.choices.length; ++i)
                    if (row.choices[i].value === row.value) selected = i
                item.currentIndex = selected
            }
            return item
        })
    }

    Repeater {
        model: groups.sections
        delegate: MeoSettingsGroup {
            required property var modelData
            width: groups.width
            title: modelData.title
            model: groups.rowsFor(modelData)
            onRowToggled: (index, checked, row) => {
                KdeConfigBackend.setValue(groups.schemaId, row.id, checked)
                groups.applied(qsTr("Setting applied"))
            }
            onRowSliderMoved: (index, value, row) => groups.queueNumber(row.id, Math.round(value))
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                KdeConfigBackend.setValue(groups.schemaId, row.id, String(row.options[optionIndex].value))
                groups.applied(qsTr("Setting applied"))
            }
        }
    }
}
