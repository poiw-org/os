import {Collection, EntityConstructorOrPath, getRepository} from 'fireorm';
import {EventType} from "../enums/EventType";

@Collection()
export default class Event {
    declare id: string;
    declare type: EventType;
    declare timestamp: number;
    declare payload: any;

    static getRepo = () =>  getRepository(Event as EntityConstructorOrPath<any>);
}