//
// The Key class is declared here but implemented as a SubCollection in the User class.
//

export default class Key{
    declare id: string;
    declare name: string;
    declare authorizationLevel: number;
    declare expirationDate: Date;
    declare isBlocked: boolean;
}