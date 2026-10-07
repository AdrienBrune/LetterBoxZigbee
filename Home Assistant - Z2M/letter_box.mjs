export default {
    fingerprint: [
        {
            manufacturerName: 'CUSTOM',
            modelID: 'LETTER-BOX',
        },
    ],

    model: 'LETTER-BOX',
    vendor: 'DIY',
    description: 'Boîte aux lettres : Porte, Clapet et Batterie',
    icon: 'device_icons/letter_box.png',

    fromZigbee: [
        {
            cluster: 'genBinaryInput',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const endpoint = msg.endpoint.ID;
                if (msg.data && msg.data.presentValue !== undefined) {
                    const status = msg.data.presentValue === 1 ? 'OPENED' : 'CLOSED';
                    if (endpoint === 1) return { door: status };
                    if (endpoint === 2) return { flapper: status };
                }
            },
        },
        {
            cluster: 'genPowerCfg',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                if (msg.data && msg.data.batteryPercentageRemaining !== undefined) {
                    result.battery = Math.round(msg.data.batteryPercentageRemaining / 2);
                }
                return result;
            },
        },
    ],

    toZigbee: [],

    exposes: [
        {
            type: 'binary',
            name: 'door',
            property: 'door',
            access: 1,
            value_on: 'OPENED',
            value_off: 'CLOSED',
            description: 'mail box door status',
        },
        {
            type: 'binary',
            name: 'flapper',
            property: 'flapper',
            access: 1, 
            value_on: 'OPENED',
            value_off: 'CLOSED',
            description: 'mail box flapper opening',
        },
        {
            type: 'numeric',
            name: 'battery',
            property: 'battery',
            access: 1,
            unit: '%',
            description: 'remaining battery in %',
        }
    ],
};